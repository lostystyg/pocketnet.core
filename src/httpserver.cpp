// Copyright (c) 2015-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <httpserver.h>

#include <chainparamsbase.h>
#include <util.h>
#include <utilstrencodings.h>
#include <netbase.h>
#include <sync.h>
#include <ui_interface.h>
#include <memory>
#include <cstdlib>
#include <deque>
#include <future>
#include <rpc/register.h>
#include <walletinitinterface.h>

#ifdef EVENT__HAVE_NETINET_IN_H
#include <netinet/in.h>
#ifdef _XOPEN_SOURCE_EXTENDED
#include <arpa/inet.h>
#endif
#endif

#if ENABLE_ZMQ
#include <zmq/zmqrpc.h>
#endif

/** Maximum size of http request (request line + headers) */
static const size_t MAX_HEADERS_SIZE = 8192;

/** Simple work queue for distributing work over multiple threads.
 * Work items are simply callable objects.
 */
template<typename WorkItem>
class WorkQueue
{
private:
    /** Mutex protects entire object */
    Mutex cs;
    std::condition_variable cond;
    std::deque<std::unique_ptr<WorkItem>> queue;
    bool running;
    size_t maxDepth;

public:
    explicit WorkQueue(size_t _maxDepth) : running(true), maxDepth(_maxDepth)
    {

    }

    /** Precondition: worker threads have all stopped (they have been joined).
     */
    ~WorkQueue()
    {
    }

    /** Enqueue a work item */
    bool Enqueue(WorkItem *item)
    {
        LOCK(cs);

        if (queue.size() >= maxDepth)
            return false;

        queue.emplace_back(std::unique_ptr<WorkItem>(item));
        cond.notify_one();

        return true;
    }

    /** Thread function */
    void Run(bool selfDbConnection)
    {
        DbConnectionRef sqliteConnection;
        if (selfDbConnection)
            sqliteConnection = std::make_shared<PocketDb::SQLiteConnection>();

        while (true)
        {
            std::unique_ptr<WorkItem> i;
            {
                WAIT_LOCK(cs, lock);
                while (running && queue.empty())
                    cond.wait(lock);
                if (!running)
                    break;
                i = std::move(queue.front());
                queue.pop_front();
            }
            (*i)(sqliteConnection);
        }
    }

    /** Interrupt and exit loops */
    void Interrupt()
    {
        LOCK(cs);
        running = false;
        cond.notify_all();
    }
};

struct HTTPPathHandler
{
    HTTPPathHandler(std::string _prefix, bool _exactMatch, HTTPRequestHandler _handler,
                    WorkQueue<HTTPClosure>* _queue) :
        prefix(_prefix), exactMatch(_exactMatch), handler(_handler), queue(_queue)
    {
    }

    std::string prefix;
    bool exactMatch;
    HTTPRequestHandler handler;
    WorkQueue<HTTPClosure>* queue;
};

/** HTTP module state */

//! libevent event loop
static struct event_base *eventBase = nullptr;
//! HTTP server
struct evhttp *eventHTTP = nullptr;
//! List of subnets to allow RPC connections from
static std::vector<CSubNet> rpc_allow_subnets;

//! HTTP socket objects to handle requests on different routes
HTTPSocket *g_socket;
HTTPWebSocket *g_webSocket;
HTTPSocket *g_staticSocket;
HTTPSocket *g_restSocket;

std::thread threadHTTP;
std::future<bool> threadResult;

/** Check if a network address is allowed to access the HTTP server */
static bool ClientAllowed(const CNetAddr &netaddr)
{
    if (!netaddr.IsValid())
        return false;
    for (const CSubNet &subnet : rpc_allow_subnets)
        if (subnet.Match(netaddr))
            return true;
    return false;
}

/** Initialize ACL list for HTTP server */
static bool InitHTTPAllowList()
{
    rpc_allow_subnets.clear();
    CNetAddr localv4;
    CNetAddr localv6;
    LookupHost("127.0.0.1", localv4, false);
    LookupHost("::1", localv6, false);
    rpc_allow_subnets.push_back(CSubNet(localv4, 8));      // always allow IPv4 local subnet
    rpc_allow_subnets.push_back(CSubNet(localv6));         // always allow IPv6 localhost
    for (const std::string &strAllow : gArgs.GetArgs("-rpcallowip"))
    {
        CSubNet subnet;
        LookupSubNet(strAllow.c_str(), subnet);
        if (!subnet.IsValid())
        {
            uiInterface.ThreadSafeMessageBox(
                strprintf(
                    "Invalid -rpcallowip subnet specification: %s. Valid are a single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0) or a network/CIDR (e.g. 1.2.3.4/24).",
                    strAllow),
                "", CClientUIInterface::MSG_ERROR);
            return false;
        }
        rpc_allow_subnets.push_back(subnet);
    }
    std::string strAllowed;
    for (const CSubNet &subnet : rpc_allow_subnets)
        strAllowed += subnet.ToString() + " ";
    LogPrint(BCLog::HTTP, "Allowing HTTP connections from: %s\n", strAllowed);
    return true;
}

/** HTTP request method as string - use for logging only */
static std::string RequestMethodString(HTTPRequest::RequestMethod m)
{
    switch (m)
    {
        case HTTPRequest::GET:
            return "GET";
        case HTTPRequest::POST:
            return "POST";
        case HTTPRequest::HEAD:
            return "HEAD";
        case HTTPRequest::PUT:
            return "PUT";
        case HTTPRequest::OPTIONS:
            return "OPTIONS";
        default:
            return "unknown";
    }
}

/** HTTP request callback */
static void http_request_cb(struct evhttp_request *req, void *arg)
{
    auto *httpSock = (HTTPSocket*) arg;
    // Disable reading to work around a libevent bug, fixed in 2.2.0.
    if (event_get_version_number() >= 0x02010600 && event_get_version_number() < 0x02020001)
    {
        evhttp_connection *conn = evhttp_request_get_connection(req);
        if (conn)
        {
            bufferevent *bev = evhttp_connection_get_bufferevent(conn);
            if (bev)
            {
                bufferevent_disable(bev, EV_READ);
            }
        }
    }
    std::unique_ptr<HTTPRequest> hreq(new HTTPRequest(req));

    LogPrint(BCLog::HTTP, "Received a %s request for %s from %s\n",
        RequestMethodString(hreq->GetRequestMethod()), hreq->GetURI(), hreq->GetPeer().ToString());

    // Early address-based allow check
    if (!httpSock->m_publicAccess && !ClientAllowed(hreq->GetPeer()))
    {
        LogPrint(BCLog::HTTP, "Request from %s not allowed\n", hreq ? hreq->GetPeer().ToString() : "unknown");
        hreq->WriteReply(HTTP_FORBIDDEN);
        return;
    }

    // Early reject unknown HTTP methods
    if (hreq->GetRequestMethod() == HTTPRequest::UNKNOWN)
    {
        hreq->WriteReply(HTTP_BADMETHOD);
        return;
    }
    
    hreq->WriteHeader("Access-Control-Allow-Origin", "*");
    hreq->WriteHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    hreq->WriteHeader("Access-Control-Allow-Headers", "*");

    if (hreq->GetRequestMethod() == HTTPRequest::OPTIONS)
    {
        hreq->WriteReply(HTTP_NOCONTENT);
        return;
    }

    // Find registered handler for prefix
    std::string strURI = hreq->GetURI();
    std::string path;
    auto i = httpSock->m_pathHandlers.begin();
    auto iend = httpSock->m_pathHandlers.end();

    for (; i != iend; ++i)
    {
        bool match = false;
        if (i->exactMatch)
            match = (strURI == i->prefix);
        else
            match = (strURI.substr(0, i->prefix.size()) == i->prefix);

        if (match)
        {
            path = strURI.substr(i->prefix.size());
            break;
        }
    }

    // Dispatch to worker thread
    if (i != iend)
    {
        std::unique_ptr<HTTPWorkItem> item(new HTTPWorkItem(std::move(hreq), path, i->handler));

        if (i->queue->Enqueue(item.get()))
        {
            item.release();
        }
        else
        {
            LogPrint(BCLog::RPCERROR, "WARNING: request rejected because http work queue depth exceeded.\n");
            item->req->WriteReply(HTTP_INTERNAL, "Work queue depth exceeded");
        }
    }
    else
    {
        LogPrint(BCLog::HTTP, "Request from %s not found\n", hreq->GetPeer().ToString());
        hreq->WriteReply(HTTP_NOTFOUND);
    }
}

/** Callback to reject HTTP requests after shutdown. */
static void http_reject_request_cb(struct evhttp_request *req, void *)
{
    LogPrint(BCLog::HTTP, "Rejecting request while shutting down\n");
    evhttp_send_error(req, HTTP_SERVUNAVAIL, nullptr);
}

/** Event dispatcher thread */
static bool ThreadHTTP(struct event_base *base)
{
    RenameThread("pocketcoin-http");
    LogPrint(BCLog::HTTP, "Entering http event loop\n");
    event_base_dispatch(base);
    // Event loop will be interrupted by InterruptHTTPServer()
    LogPrint(BCLog::HTTP, "Exited http event loop\n");
    return event_base_got_break(base) == 0;
}

/** Bind HTTP server to specified addresses */
static bool HTTPBindAddresses()
{
    int securePort = gArgs.GetArg("-rpcport", BaseParams().RPCPort());
    int publicPort = gArgs.GetArg("-publicrpcport", BaseParams().PublicRPCPort());
    int staticPort = gArgs.GetArg("-staticrpcport", BaseParams().StaticRPCPort());
    int restPort = gArgs.GetArg("-restport", BaseParams().RestPort());
    int bindAddresses = 0;

    // Determine what addresses to bind to
    if (g_socket)
    {
        if (!gArgs.IsArgSet("-rpcallowip"))
        { // Default to loopback if not allowing external IPs
            g_socket->BindAddress("::1", securePort);
            g_socket->BindAddress("127.0.0.1", securePort);
            if (gArgs.IsArgSet("-rpcbind"))
            {
                LogPrintf(
                    "WARNING: option -rpcbind was ignored because -rpcallowip was not specified, refusing to allow everyone to connect\n");
            }
        }
        else if (gArgs.IsArgSet("-rpcbind"))
        { // Specific bind address
            for (const std::string& strRPCBind: gArgs.GetArgs("-rpcbind"))
            {
                std::string host;
                int port = securePort;
                SplitHostPort(strRPCBind, port, host);
                g_socket->BindAddress(host, port);
            }
        }
        else
        { // No specific bind address specified, bind to any
            g_socket->BindAddress("::", securePort);
            g_socket->BindAddress("0.0.0.0", securePort);
        }

        bindAddresses += g_socket->GetAddressCount();
    }

    // Public sockets always bind to any IPs
    if (g_webSocket)
    {
        g_webSocket->BindAddress("::", publicPort);
        g_webSocket->BindAddress("0.0.0.0", publicPort);
    }
    if (g_staticSocket)
    {
        g_staticSocket->BindAddress("::", staticPort);
        g_staticSocket->BindAddress("0.0.0.0", staticPort);
    }
    if (g_restSocket)
    {
        g_restSocket->BindAddress("::", restPort);
        g_restSocket->BindAddress("0.0.0.0", restPort);
    }

    return bindAddresses;
}

/** Simple wrapper to set thread name and run work queue */
static void HTTPWorkQueueRun(WorkQueue<HTTPClosure> *queue, bool selfDbConnection)
{
    RenameThread("pocketcoin-httpworker");
    queue->Run(selfDbConnection);
}

/** libevent event log callback */
static void libevent_log_cb(int severity, const char *msg)
{
#ifndef EVENT_LOG_WARN
    // EVENT_LOG_WARN was added in 2.0.19; but before then _EVENT_LOG_WARN existed.
# define EVENT_LOG_WARN _EVENT_LOG_WARN
#endif
    if (severity >= EVENT_LOG_WARN) // Log warn messages and higher without debug category
        LogPrintf("libevent: %s\n", msg);
    else
        LogPrint(BCLog::LIBEVENT, "libevent: %s\n", msg);
}

using namespace std::chrono;

static void JSONErrorReply(HTTPRequest* req, const UniValue& objError, const UniValue& id)
{
    // Send error reply from json-rpc error object
    int nStatus = HTTP_INTERNAL_SERVER_ERROR;
    int code = find_value(objError, "code").get_int();

    if (code == RPC_INVALID_REQUEST)
        nStatus = HTTP_BAD_REQUEST;
    else if (code == RPC_METHOD_NOT_FOUND)
        nStatus = HTTP_NOT_FOUND;

    std::string strReply = JSONRPCReply(NullUniValue, objError, id);

    req->WriteHeader("Content-Type", "application/json");
    req->WriteReply(nStatus, strReply);
}

bool InitHTTPServer()
{
    if (!InitHTTPAllowList())
        return false;

    // Redirect libevent's logging to our own log
    event_set_log_callback(&libevent_log_cb);
    // Update libevent's log handling. Returns false if our version of
    // libevent doesn't support debug logging, in which case we should
    // clear the BCLog::LIBEVENT flag.
    if (!UpdateHTTPServerLogging(g_logger->WillLogCategory(BCLog::LIBEVENT)))
    {
        g_logger->DisableCategory(BCLog::LIBEVENT);
    }

#ifdef WIN32
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif
    
    int timeout = gArgs.GetArg("-rpcservertimeout", DEFAULT_HTTP_SERVER_TIMEOUT);
    int workQueueMainDepth = std::max((long) gArgs.GetArg("-rpcworkqueue", DEFAULT_HTTP_WORKQUEUE), 1L);
    int workQueuePostDepth = std::max((long) gArgs.GetArg("-rpcpostworkqueue", DEFAULT_HTTP_POST_WORKQUEUE), 1L);
    int workQueuePublicDepth = std::max((long) gArgs.GetArg("-rpcpublicworkqueue", DEFAULT_HTTP_PUBLIC_WORKQUEUE), 1L);
    int workQueueStaticDepth = std::max((long) gArgs.GetArg("-rpcstaticworkqueue", DEFAULT_HTTP_STATIC_WORKQUEUE), 1L);
    int workQueueRestDepth = std::max((long) gArgs.GetArg("-rpcrestworkqueue", DEFAULT_HTTP_REST_WORKQUEUE), 1L);

    raii_event_base base_ctr = obtain_event_base();
    eventBase = base_ctr.get();

    // General private socket
    g_socket = new HTTPSocket(eventBase, timeout, workQueueMainDepth, false);
    RegisterBlockchainRPCCommands(g_socket->m_table_rpc);
    RegisterNetRPCCommands(g_socket->m_table_rpc);
    RegisterMiscRPCCommands(g_socket->m_table_rpc);
    RegisterMiningRPCCommands(g_socket->m_table_rpc);
    RegisterRawTransactionRPCCommands(g_socket->m_table_rpc);
    g_wallet_init_interface.RegisterRPC(g_socket->m_table_rpc);
#if ENABLE_ZMQ
    RegisterZMQRPCCommands(g_socket->m_table_rpc);
#endif

    // Additional pocketnet seocket
    if (gArgs.GetBoolArg("-api", false))
    {
        g_webSocket = new HTTPWebSocket(eventBase, timeout, workQueuePublicDepth, workQueuePostDepth, true);
        RegisterPocketnetWebRPCCommands(g_webSocket->m_table_rpc, g_webSocket->m_table_post_rpc);

        // Additional pocketnet static files socket
        g_staticSocket = new HTTPSocket(eventBase, timeout, workQueueStaticDepth, true);
        g_restSocket = new HTTPSocket(eventBase, timeout, workQueueRestDepth, true);
    }
 
    if (!HTTPBindAddresses())
    {
        LogPrintf("Unable to bind any endpoint for RPC server\n");
        return false;
    }

    LogPrint(BCLog::HTTP, "Initialized HTTP server\n");

    // transfer ownership to eventBase/HTTP via .release()
    eventBase = base_ctr.release();
    return true;
}

bool UpdateHTTPServerLogging(bool enable)
{
#if LIBEVENT_VERSION_NUMBER >= 0x02010100
    if (enable)
    {
        event_enable_debug_logging(EVENT_DBG_ALL);
    } else
    {
        event_enable_debug_logging(EVENT_DBG_NONE);
    }
    return true;
#else
    // Can't update libevent logging if version < 02010100
    return false;
#endif
}

void StartHTTPServer()
{
    LogPrint(BCLog::HTTP, "Starting HTTP server\n");
    int rpcMainThreads = std::max((long) gArgs.GetArg("-rpcthreads", DEFAULT_HTTP_THREADS), 1L);
    int rpcPostThreads = std::max((long) gArgs.GetArg("-rpcpostthreads", DEFAULT_HTTP_POST_THREADS), 1L);
    int rpcPublicThreads = std::max((long) gArgs.GetArg("-rpcpublicthreads", DEFAULT_HTTP_PUBLIC_THREADS), 1L);
    int rpcStaticThreads = std::max((long) gArgs.GetArg("-rpcstaticthreads", DEFAULT_HTTP_STATIC_THREADS), 1L);
    int rpcRestThreads = std::max((long) gArgs.GetArg("-rpcrestthreads", DEFAULT_HTTP_REST_THREADS), 1L);

    std::packaged_task<bool(event_base *)> task(ThreadHTTP);
    threadResult = task.get_future();
    threadHTTP = std::thread(std::move(task), eventBase);

    if (g_socket)
    {
        g_socket->StartHTTPSocket(rpcMainThreads, false);
        LogPrintf("HTTP: starting %d Main worker threads\n", rpcMainThreads);
    }

    // The same worker threads will service POST and PUBLIC RPC requests
    if (g_webSocket)
    {
        g_webSocket->StartHTTPSocket(rpcPublicThreads, rpcPostThreads, true);
        LogPrintf("HTTP: starting %d Public worker threads\n", rpcPublicThreads);
    }
    if (g_staticSocket)
    {
        g_staticSocket->StartHTTPSocket(rpcStaticThreads, false);
        LogPrintf("HTTP: starting %d Static worker threads\n", rpcStaticThreads);
    }
    if (g_restSocket)
    {
        g_restSocket->StartHTTPSocket(rpcRestThreads, true);
        LogPrintf("HTTP: starting %d Rest worker threads\n", rpcRestThreads);
    }
}

void InterruptHTTPServer()
{
    LogPrint(BCLog::HTTP, "Interrupting HTTP server\n");
    if (g_socket) g_socket->InterruptHTTPSocket();
    if (g_webSocket) g_webSocket->InterruptHTTPSocket();
    if (g_staticSocket) g_staticSocket->InterruptHTTPSocket();
    if (g_restSocket) g_restSocket->InterruptHTTPSocket();
}

void StopHTTPServer()
{
    LogPrint(BCLog::HTTP, "Stopping HTTP server\n");

    LogPrint(BCLog::HTTP, "Waiting for HTTP worker threads to exit\n");
    if (g_socket) g_socket->StopHTTPSocket();
    if (g_webSocket) g_webSocket->StopHTTPSocket();
    if (g_staticSocket) g_staticSocket->StopHTTPSocket();
    if (g_restSocket) g_restSocket->StopHTTPSocket();

    if (eventBase)
    {
        LogPrint(BCLog::HTTP, "Waiting for HTTP event thread to exit\n");
        // Exit the event loop as soon as there are no active events.
        event_base_loopexit(eventBase, nullptr);

        // Give event loop a few seconds to exit (to send back last RPC responses), then break it
        // Before this was solved with event_base_loopexit, but that didn't work as expected in
        // at least libevent 2.0.21 and always introduced a delay. In libevent
        // master that appears to be solved, so in the future that solution
        // could be used again (if desirable).
        // (see discussion in https://github.com/pocketcoin/pocketcoin/pull/6990)
        if (threadResult.valid() &&
            threadResult.wait_for(std::chrono::milliseconds(2000)) == std::future_status::timeout)
        {
            LogPrintf("HTTP event loop did not exit within allotted time, sending loopbreak\n");
            event_base_loopbreak(eventBase);
        }

        threadHTTP.join();
    }

    delete g_socket;
    g_socket = nullptr;

    delete g_webSocket;
    g_webSocket = nullptr;

    delete g_staticSocket;
    g_staticSocket = nullptr;

    delete g_restSocket;
    g_restSocket = nullptr;

    if (eventBase)
    {
        event_base_free(eventBase);
        eventBase = nullptr;
    }
    LogPrint(BCLog::HTTP, "Stopped HTTP server\n");
}

struct event_base *EventBase()
{
    return eventBase;
}

static void httpevent_callback_fn(evutil_socket_t, short, void *data)
{
    // Static handler: simply call inner handler
    HTTPEvent *self = static_cast<HTTPEvent *>(data);
    self->handler();
    if (self->deleteWhenTriggered)
        delete self;
}

HTTPSocket::HTTPSocket(struct event_base *base, int timeout, int queueDepth, bool publicAccess):
    m_http(nullptr), m_eventHTTP(nullptr), m_workQueue(nullptr), m_publicAccess(publicAccess)
{
    /* Create a new evhttp object to handle requests. */
    raii_evhttp http_ctr = obtain_evhttp(base);
    m_http = http_ctr.get();
    if (!m_http)
    {
        LogPrintf("couldn't create evhttp. Exiting.\n");
        return;
    }

    evhttp_set_timeout(m_http, gArgs.GetArg("-rpcservertimeout", DEFAULT_HTTP_SERVER_TIMEOUT));
    evhttp_set_max_headers_size(m_http, MAX_HEADERS_SIZE);
    evhttp_set_max_body_size(m_http, MAX_SIZE);
    evhttp_set_gencb(m_http, http_request_cb, (void*) this);
    evhttp_set_allowed_methods(m_http,
        evhttp_cmd_type::EVHTTP_REQ_GET |
        evhttp_cmd_type::EVHTTP_REQ_POST |
        evhttp_cmd_type::EVHTTP_REQ_HEAD |
        evhttp_cmd_type::EVHTTP_REQ_PUT |
        evhttp_cmd_type::EVHTTP_REQ_DELETE |
        evhttp_cmd_type::EVHTTP_REQ_OPTIONS
    );

    m_workQueue = new WorkQueue<HTTPClosure>(queueDepth);
    LogPrintf("HTTP: creating work queue of depth %d\n", queueDepth);

    // transfer ownership to eventBase/HTTP via .release()
    m_eventHTTP = http_ctr.release(); 
}

HTTPSocket::~HTTPSocket()
{
    if (m_eventHTTP)
    {
        evhttp_free(m_eventHTTP);
        m_eventHTTP = nullptr;
    }
}

void HTTPSocket::StartThreads(WorkQueue<HTTPClosure>* queue, int threadCount, bool selfDbConnection)
{
    for (int i = 0; i < threadCount; i++)
        m_thread_http_workers.emplace_back(HTTPWorkQueueRun, queue, selfDbConnection);
}

void HTTPSocket::StartHTTPSocket(int threadCount, bool selfDbConnection)
{
    StartThreads(m_workQueue, threadCount, selfDbConnection);
}

void HTTPSocket::StopHTTPSocket()
{
    LogPrint(BCLog::HTTP, "Waiting for HTTP worker threads to exit\n");

    for (auto &thread: m_thread_http_workers)
        thread.join();

    m_thread_http_workers.clear();
    
    delete m_workQueue;
    m_workQueue = nullptr;
}

void HTTPSocket::InterruptHTTPSocket()
{
    if (m_eventHTTP)
    {
        // Unlisten sockets
        for (evhttp_bound_socket *socket : m_boundSockets)
        {
            evhttp_del_accept_socket(m_eventHTTP, socket);
        }
        // Reject requests on current connections
        evhttp_set_gencb(m_eventHTTP, http_reject_request_cb, nullptr);
    }

    if (m_workQueue)
        m_workQueue->Interrupt();
}

void HTTPSocket::BindAddress(std::string ipAddr, int port)
{ 
    LogPrint(BCLog::HTTP, "Binding RPC on address %s port %i\n", ipAddr, port);
    evhttp_bound_socket *bind_handle = evhttp_bind_socket_with_handle(m_eventHTTP, ipAddr.empty() ? nullptr : ipAddr.c_str(), port);
    if (bind_handle)
    {
        m_boundSockets.push_back(bind_handle);
    }
    else
    {
        LogPrint(BCLog::HTTP,"Binding RPC on address %s port %i failed.\n", ipAddr, port);
    }
}

int HTTPSocket::GetAddressCount()
{
    return (int)m_boundSockets.size();
}

void HTTPSocket::RegisterHTTPHandler(const std::string &prefix, bool exactMatch,
                                     const HTTPRequestHandler &handler, WorkQueue<HTTPClosure>* _queue)
{
    LogPrint(BCLog::HTTP, "Registering HTTP handler for %s (exactmatch %d)\n", prefix, exactMatch);
    m_pathHandlers.emplace_back(prefix, exactMatch, handler, _queue);
}

void HTTPSocket::UnregisterHTTPHandler(const std::string &prefix, bool exactMatch)
{
    auto i = m_pathHandlers.begin();
    auto iend = m_pathHandlers.end();
    for (; i != iend; ++i)
        if (i->prefix == prefix && i->exactMatch == exactMatch)
            break;
    if (i != iend)
    {
        LogPrint(BCLog::HTTP, "Unregistering HTTP handler for %s (exactmatch %d)\n", prefix, exactMatch);
        m_pathHandlers.erase(i);
    }
}

static inline std::string gen_random(const int len) {

    std::string tmp_s;
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    srand( (unsigned) time(NULL) * getpid());

    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i)
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];


    return tmp_s;

}

bool HTTPSocket::HTTPReq(HTTPRequest* req, CRPCTable& table)
{
    // JSONRPC handles only POST
    if (req->GetRequestMethod() != HTTPRequest::POST) {
        LogPrint(BCLog::RPCERROR, "WARNING: Request not POST\n");
        req->WriteReply(HTTP_BAD_METHOD, "JSONRPC server handles only POST requests");
        return false;
    }

    string uri;
    string method;
    string peer;
    auto start = gStatEngineInstance.GetCurrentSystemTime();
    bool executeSuccess = true;

    JSONRPCRequest jreq;
    try
    {
        UniValue valRequest;

        if (!valRequest.read(req->ReadBody()))
            throw JSONRPCError(RPC_PARSE_ERROR, "Parse error");

        // Set the URI
        jreq.URI = req->GetURI();
        std::string strReply;

        // singleton request
        if (valRequest.isObject())
        {
            jreq.parse(valRequest);
            jreq.SetDbConnection(req->DbConnection());

            uri = jreq.URI;
            method = jreq.strMethod;
            peer = jreq.peerAddr.substr(0, jreq.peerAddr.find(':'));
            string prms = jreq.params.write(0, 0);

            auto rpcKey = gen_random(15);
            LogPrint(BCLog::RPC, "RPC started method %s%s (%s) with params: %s\n",
                uri, method, rpcKey, prms);

            UniValue result = table.execute(jreq);

            auto execute = gStatEngineInstance.GetCurrentSystemTime();

            LogPrint(BCLog::RPC, "RPC executed method %s%s (%s) > %.2fms\n",
                uri, method, rpcKey, (execute.count() - start.count()));

            // Send reply
            strReply = JSONRPCReply(result, NullUniValue, jreq.id);
        }
        else
        {
            if (valRequest.isArray())
            {
                strReply = JSONRPCExecBatch(jreq, valRequest.get_array(), table);
            }
            else
            {
                throw JSONRPCError(RPC_PARSE_ERROR, "Top-level object parse error");
            }
        }

        req->WriteHeader("Content-Type", "application/json");
        req->WriteReply(HTTP_OK, strReply);
    }
    catch (const UniValue& objError)
    {
        LogPrint(BCLog::RPCERROR, "Exception %s\n", objError.write());
        JSONErrorReply(req, objError, jreq.id);
        executeSuccess = false;
    }
    catch (const std::exception& e)
    {
        LogPrint(BCLog::RPCERROR, "Exception 2 %s\n", JSONRPCError(RPC_PARSE_ERROR, e.what()).write());
        JSONErrorReply(req, JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
        executeSuccess = false;
    }

    // Collect statistic data
    if (g_logger->WillLogCategory(BCLog::STAT))
    {
        auto finish = gStatEngineInstance.GetCurrentSystemTime();

        gStatEngineInstance.AddSample(
            Statistic::RequestSample{
                uri,
                req->Created,
                start,
                finish,
                peer,
                !executeSuccess,
                0,
                0
            }
        );
    }

    return executeSuccess;
}

/** WebSocket for public API */
HTTPWebSocket::HTTPWebSocket(struct event_base* base, int timeout, int queueDepth, int queuePostDepth, bool publicAccess)
    : HTTPSocket(base, timeout, queueDepth, publicAccess)
{
    m_workPostQueue = new WorkQueue<HTTPClosure>(queuePostDepth);
    LogPrintf("HTTP: creating work post queue of depth %d\n", queuePostDepth);
}

HTTPWebSocket::~HTTPWebSocket() = default;

void HTTPWebSocket::StartHTTPSocket(int threadCount, int threadPostCount, bool selfDbConnection)
{
    StartThreads(m_workQueue, threadCount, selfDbConnection);
    StartThreads(m_workPostQueue, threadPostCount, selfDbConnection);
}

void HTTPWebSocket::StopHTTPSocket()
{   
    HTTPSocket::StopHTTPSocket();

    delete m_workPostQueue;
    m_workPostQueue = nullptr;
}

void HTTPWebSocket::InterruptHTTPSocket()
{
    HTTPSocket::InterruptHTTPSocket();

    if (m_workPostQueue)
        m_workPostQueue->Interrupt();
}

HTTPEvent::HTTPEvent(struct event_base *base, bool _deleteWhenTriggered, std::function<void()> _handler) :
    deleteWhenTriggered(_deleteWhenTriggered), handler(std::move(_handler))
{
    ev = event_new(base, -1, 0, httpevent_callback_fn, this);
    assert(ev);
}

HTTPEvent::~HTTPEvent()
{
    event_free(ev);
}

void HTTPEvent::trigger(struct timeval *tv)
{
    if (tv == nullptr)
        event_active(ev, 0, 0); // immediately trigger event in main thread
    else
        evtimer_add(ev, tv); // trigger after timeval passed
}

HTTPRequest::HTTPRequest(struct evhttp_request *_req) : req(_req),
                                                        replySent(false)
{
    Created = gStatEngineInstance.GetCurrentSystemTime();
}
HTTPRequest::~HTTPRequest()
{
    if (!replySent)
    {
        // Keep track of whether reply was sent to avoid request leaks
        LogPrintf("%s: Unhandled request\n", __func__);
        WriteReply(HTTP_INTERNAL, "Unhandled request");
    }
    // evhttpd cleans up the request, as long as a reply was sent.
}

std::pair<bool, std::string> HTTPRequest::GetHeader(const std::string &hdr) const
{
    const struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
    assert(headers);
    const char *val = evhttp_find_header(headers, hdr.c_str());
    if (val)
        return std::make_pair(true, val);
    else
        return std::make_pair(false, "");
}

std::string HTTPRequest::ReadBody()
{
    struct evbuffer *buf = evhttp_request_get_input_buffer(req);
    if (!buf)
        return "";
    size_t size = evbuffer_get_length(buf);
    /** Trivial implementation: if this is ever a performance bottleneck,
     * internal copying can be avoided in multi-segment buffers by using
     * evbuffer_peek and an awkward loop. Though in that case, it'd be even
     * better to not copy into an intermediate string but use a stream
     * abstraction to consume the evbuffer on the fly in the parsing algorithm.
     */
    const char *data = (const char *) evbuffer_pullup(buf, size);
    if (!data) // returns nullptr in case of empty buffer
        return "";
    std::string rv(data, size);
    evbuffer_drain(buf, size);
    return rv;
}

void HTTPRequest::WriteHeader(const std::string &hdr, const std::string &value)
{
    struct evkeyvalq *headers = evhttp_request_get_output_headers(req);
    assert(headers);
    evhttp_add_header(headers, hdr.c_str(), value.c_str());
}

/** Closure sent to main thread to request a reply to be sent to
 * a HTTP request.
 * Replies must be sent in the main loop in the main http thread,
 * this cannot be done from worker threads.
 */
void HTTPRequest::WriteReply(int nStatus, const std::string &strReply)
{
    assert(!replySent && req);

    // Send event to main http thread to send reply message
    struct evbuffer *evb = evhttp_request_get_output_buffer(req);
    assert(evb);
    evbuffer_add(evb, strReply.data(), strReply.size());
    auto req_copy = req;
    auto *ev = new HTTPEvent(eventBase, true, [req_copy, nStatus]
    {
        evhttp_send_reply(req_copy, nStatus, nullptr, nullptr);
        // Re-enable reading from the socket. This is the second part of the libevent
        // workaround above.
        if (event_get_version_number() >= 0x02010600 && event_get_version_number() < 0x02020001)
        {
            evhttp_connection *conn = evhttp_request_get_connection(req_copy);
            if (conn)
            {
                bufferevent *bev = evhttp_connection_get_bufferevent(conn);
                if (bev)
                {
                    bufferevent_enable(bev, EV_READ | EV_WRITE);
                }
            }
        }
    });
    ev->trigger(nullptr);
    replySent = true;
    req = nullptr; // transferred back to main thread
}

void HTTPRequest::SetDbConnection(const DbConnectionRef& _dbConnection)
{
    dbConnection = _dbConnection;
}

const DbConnectionRef& HTTPRequest::DbConnection() const
{
    return dbConnection;
}

CService HTTPRequest::GetPeer() const
{
    evhttp_connection *con = evhttp_request_get_connection(req);
    CService peer;
    if (con)
    {
        // evhttp retains ownership over returned address string
        const char *address = "";
        uint16_t port = 0;
        evhttp_connection_get_peer(con, (char **) &address, &port);
        peer = LookupNumeric(address, port);
    }
    return peer;
}

std::string HTTPRequest::GetURI() const
{
    return evhttp_request_get_uri(req);
}

HTTPRequest::RequestMethod HTTPRequest::GetRequestMethod() const
{
    switch (evhttp_request_get_command(req))
    {
        case EVHTTP_REQ_GET:
            return GET;
            break;
        case EVHTTP_REQ_POST:
            return POST;
            break;
        case EVHTTP_REQ_HEAD:
            return HEAD;
            break;
        case EVHTTP_REQ_PUT:
            return PUT;
            break;
        case EVHTTP_REQ_OPTIONS:
            return OPTIONS;
            break;
        default:
            return UNKNOWN;
            break;
    }
}

std::string urlDecode(const std::string &urlEncoded)
{
    std::string res;
    if (!urlEncoded.empty())
    {
        char *decoded = evhttp_uridecode(urlEncoded.c_str(), false, nullptr);
        if (decoded)
        {
            res = std::string(decoded);
            free(decoded);
        }
    }
    return res;
}
