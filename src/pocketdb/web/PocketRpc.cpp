// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/web/PocketRpc.h"

UniValue gettemplate(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            "getrawtransactionwithmessagebyid\n"
            "\nReturn Pocketnet posts.\n");

    UniValue aResult(UniValue::VARR);
    return aResult;
}

// @formatter:off
static const CRPCCommand commands[] =
{
    // Old methods
    {"artifacts", "getrecommendedposts",              &gettemplate,                       {"address", "count", "height", "lang", "contenttypes"}},
    {"artifacts", "getusercontents",                  &gettemplate,                       {"address", "height", "start_txid", "count", "lang", "tags", "contenttypes"}},
    {"artifacts", "getrecomendedsubscriptionsforuser",&gettemplate,                       {"address", "count"}},

    // Search
    {"search",          "search",                           &Search,                        {"keyword", "type", "topBlock", "pageStart", "pageSize", "address"}},
    {"search",          "searchlinks",                      &SearchLinks,                   {"links", "contenttypes", "height", "count"}},

    // WebSocket
    { "websocket",      "getmissedinfo",                    &GetMissedInfo,                 {"address", "blocknumber"}},

    // Contents
    {"contents",        "gethotposts",                      &GetHotPosts,                   {"count", "depth", "height", "lang", "contenttypes", "address"}},
    { "contents",       "gethistoricalfeed",                &GetHistoricalFeed,             {"topHeight","topContentHash","countOut","lang","tags","contentTypes","txIdsExcluded","adrsExcluded","tagsExcluded","address"}},
    { "contents",       "gethistoricalstrip",               &GetHistoricalFeed,             {"topHeight","topContentHash","countOut","lang","tags","contentTypes","txIdsExcluded","adrsExcluded","tagsExcluded","address"}},
    { "contents",       "gethierarchicalfeed",              &GetHierarchicalFeed,           {"topHeight","topContentHash","countOut","lang","tags","contentTypes","txIdsExcluded","adrsExcluded","tagsExcluded","address"}},
    { "contents",       "gethierarchicalstrip",             &GetHierarchicalFeed,           {"topHeight","topContentHash","countOut","lang","tags","contentTypes","txIdsExcluded","adrsExcluded","tagsExcluded","address"}},
    {"contents",        "getrawtransactionwithmessagebyid", &GetContent,                    {"ids", "address"}},
    {"contents",        "getcontent",                       &GetContent,                    {"ids", "address"}},
    {"contents",        "getrawtransactionwithmessage",     &FeedSelector,                  {"address_from", "address_to", "start_txid", "count", "lang", "tags", "contenttypes"}},
    {"contents",        "getprofilefeed",                   &GetProfileFeed,                {"address_from", "address_to", "start_txid", "count", "lang", "tags", "contenttypes"}},
    {"contents",        "getsubscribesfeed",                &GetSubscribesFeed,             {"address_from", "address_to", "start_txid", "count", "lang", "tags", "contenttypes"}},

    // Tags
//    {"artifacts", "searchtags",                       &gettemplate,                       {"search_string", "count"}},
    { "tags",           "gettags",                          &GetTags,                       {"address", "count", "height", "lang"}},

    // Comments
    {"comments",        "getcomments",                      &GetCommentsByPost,             {"postid", "parentid", "address", "ids"}},
    {"comments",        "getlastcomments",                  &GetLastComments,               {"count", "address"}},

    // Accounts
    { "accounts",       "getuserprofile",                   &GetAccountProfiles,            {"addresses", "short"}},
    { "accounts",       "getuseraddress",                   &GetUserAddress,                {"name"}},
    { "accounts",       "getaddressregistration",           &GetAddressRegistration,        {"addresses"}},
    { "accounts",       "getuserstate",                     &GetUserState,                  {"address"}},
    { "accounts",       "txunspent",                        &GetUnspents,                   {"addresses", "minconf", "maxconf", "include_unsafe", "query_options"}},
    { "accounts",       "getaddressid",                     &GetAddressId,                  {"address_or_id"}},

    // Scores
    {"scores",          "getaddressscores",                 &GetAddressScores,              {"address", "txs"}},
    {"scores",          "getpostscores",                    &GetPostScores,                 {"txs", "address"}},
    {"scores",          "getpagescores",                    &GetPageScores,                 {"txs", "address", "cmntids"}},

    // Explorer
    { "explorer",       "getstatistic",                     &GetStatistic,                  {"endTime", "depth"}},
    { "explorer",       "getaddressspent",                  &GetAddressSpent,               {"address"}},
    { "explorer",       "getcompactblock",                  &GetCompactBlock,               {"blockHash"}},
    { "explorer",       "getlastblocks",                    &GetLastBlocks,                 {"count", "lastHeight", "verbose"}},
    { "explorer",       "searchbyhash",                     &SearchByHash,                  {"value"}},
    { "explorer",       "gettransactions",                  &GetTransactions,               {"transactions"}},
    { "explorer",       "getaddresstransactions",           &GetAddressTransactions,        {"address"}},
    { "explorer",       "getblocktransactions",             &GetBlockTransactions,          {"blockHash"}},

    // System
    { "system",         "getpeerinfo",                      &GetPeerInfo,                   {}},
    { "system",         "getnodeinfo",                      &GetNodeInfo,                   {}},
    { "system",         "gettime",                          &GetTime,                       {}},

    { "transaction",    "sendrawtransactionwithmessage",    &AddTransaction,                {"hexstring", "message", "type"}},
    { "transaction",    "addtransaction",                   &AddTransaction,                {"hexstring", "message", "type"}},
    { "transaction",    "getrawtransaction",                &GetTransaction,                {"txid"}},
};
// @formatter:on

void RegisterPocketnetWebRPCCommands(CRPCTable& t)
{
    for (const auto& command : commands)
        t.appendCommand(command.name, &command);
}
