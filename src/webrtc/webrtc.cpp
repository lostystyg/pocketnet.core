
#include "webrtc/webrtc.h"


WebRTC::WebRTC(std::shared_ptr <IRequestProcessor> requestProcessor, int port)
    : m_protocol(std::make_shared<WebRTCProtocol>(requestProcessor)),
      m_port(port)
{}


void WebRTC::InitiateNewSignalingConnection(const std::string& ip)
{
    if (m_wsConnections.has(ip)) {
        // TODO (losty-rtc): error
        return;
    }
    auto ws = std::make_shared<rtc::WebSocket>();
    ws->onOpen([ws]() {
        UniValue registermeMsg(UniValue::VOBJ);
        registermeMsg.pushKV("type", "registerme");
        ws->send(registermeMsg.write());
    });
    ws->onMessage([ws, protocol = m_protocol](rtc::message_variant data) {
        if (!std::holds_alternative<std::string>(data))
            return; // TODO (losty-rtc): error
        UniValue message(UniValue::VOBJ);
        message.read(std::get<std::string>(data));
        if (!message.exists("type")) {
            // TODO (losty-rtc): error
            return;
        }
        auto type = message["type"].get_str();
        if (type != "protocol") {
            // TODO (losty-rtc): error
            return;
        }

        if (!message.exists("ip")) {
            // TODO (losty-rtc): error
            return;
        }
        auto ip = message["ip"].get_str();
        if (!message.exists("message")) {
            // TODO (losty-rtc): error
            return;
        }


        protocol->Process(message["message"], ip, ws);
    });
    ws->onClosed([ip, &wsConnections = m_wsConnections]() {
        wsConnections.erase(ip);
    });

    std::string url = "ws://" + ip + ":" + std::to_string(m_port);
    ws->open(url);

    m_wsConnections.insert(ip, std::move(ws));
}
