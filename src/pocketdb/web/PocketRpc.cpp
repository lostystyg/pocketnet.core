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
    {"hidden",       "generatepocketnettransaction",      &GenerateTransaction,             {"address", "privKey", "outCount", "type", "payload"}},

    // Old methods
    {"artifacts", "getrecommendedposts",              &gettemplate,                       {"address", "count", "height", "lang", "contenttypes"}},
    {"artifacts", "getusercontents",                  &gettemplate,                       {"address", "height", "start_txid", "count", "lang", "tags", "contenttypes"}},
    {"artifacts", "getrecomendedsubscriptionsforuser",&gettemplate,                       {"address", "count"}},

    // Search
    {"search",          "search",                           &Search,                        {"keyword", "type", "topBlock", "pageStart", "pageSize", "address"}},
    {"search",          "searchlinks",                      &SearchLinks,                   {"links", "contenttypes", "height", "count"}},
    {"search",          "searchusers",                      &SearchUsers,                   {"keyword", "fieldtypes", "orderbyrank"}},

    // WebSocket
    {"websocket",       "getmissedinfo",                    &GetMissedInfo,                 {"address", "blocknumber"}},

    // Contents
    {"contents",        "gethotposts",                      &GetHotPosts,                   {"count", "depth", "height", "lang", "contenttypes", "address"}},
    {"contents",        "gethistoricalfeed",                &GetHistoricalFeed,             {"topHeight","topContentHash","countOut","lang","tags","contentTypes","txIdsExcluded","adrsExcluded","tagsExcluded","address"}},
    {"contents",        "gethistoricalstrip",               &GetHistoricalFeed,             {"topHeight","topContentHash","countOut","lang","tags","contentTypes","txIdsExcluded","adrsExcluded","tagsExcluded","address"}},
    {"contents",        "gethierarchicalfeed",              &GetHierarchicalFeed,           {"topHeight","topContentHash","countOut","lang","tags","contentTypes","txIdsExcluded","adrsExcluded","tagsExcluded","address"}},
    {"contents",        "gethierarchicalstrip",             &GetHierarchicalFeed,           {"topHeight","topContentHash","countOut","lang","tags","contentTypes","txIdsExcluded","adrsExcluded","tagsExcluded","address"}},
    {"contents",        "getrawtransactionwithmessagebyid", &GetContent,                    {"ids", "address"}},
    {"contents",        "getcontent",                       &GetContent,                    {"ids", "address"}},
    {"contents",        "getrawtransactionwithmessage",     &FeedSelector,                  {"address_from", "address_to", "start_txid", "count", "lang", "tags", "contenttypes"}},
    {"contents",        "getprofilefeed",                   &GetProfileFeed,                {"address_from", "address_to", "start_txid", "count", "lang", "tags", "contenttypes"}},
    {"contents",        "getsubscribesfeed",                &GetSubscribesFeed,             {"address_from", "address_to", "start_txid", "count", "lang", "tags", "contenttypes"}},
    {"contents",        "getcontentsstatistic",             &GetContentsStatistic,          {"addresses", "contentTypes", "height", "depth"}},
    {"contents",        "getcontents",                      &GetContents,                   {"address"}},

    // Tags
//    {"artifacts", "searchtags",                       &gettemplate,                       {"search_string", "count"}},
    {"tags",           "gettags",                          &GetTags,                        {"address", "count", "height", "lang"}},

    // Comments
    {"comments",        "getcomments",                      &GetCommentsByPost,             {"postid", "parentid", "address", "ids"}},
    {"comments",        "getlastcomments",                  &GetLastComments,               {"count", "address"}},

    // Accounts
    {"accounts",        "getuserprofile",                   &GetAccountProfiles,             {"addresses", "short"}},
    {"accounts",        "getuseraddress",                   &GetAccountAddress,              {"name"}},
    {"accounts",        "getaddressregistration",           &GetAddressRegistration,         {"addresses"}},
    {"accounts",        "getuserstate",                     &GetAccountState,                {"address"}},
    {"accounts",        "txunspent",                        &GetAccountUnspents,             {"addresses", "minconf", "maxconf", "include_unsafe", "query_options"}},
    {"accounts",        "getaddressid",                     &GetAccountId,                   {"address_or_id"}},
    {"accounts",        "getaccountsetting",                &GetAccountSetting,              {"address"}},
    {"accounts",        "getuserstatistic",                 &GetAccountStatistic,            {"address", "height", "depth"}},
    {"accounts",        "getusersubscribes",                &GetAccountSubscribes,           {"address", "height", "depth"}},
    {"accounts",        "getusersubscribers",               &GetAccountSubscribers,          {"address", "height", "depth"}},
    {"accounts",        "getuserblockings",                 &GetAccountBlockings,            {"address", "height", "depth"}},

    // Scores
    {"scores",          "getaddressscores",                 &GetAddressScores,              {"address", "txs"}},
    {"scores",          "getpostscores",                    &GetPostScores,                 {"txHash"}},
    {"scores",          "getpagescores",                    &GetPagesScores,                {"postIds", "address", "cmntIds"}},

    // Explorer
    {"explorer",       "getstatistic",                     &GetStatistic,                   {"endTime", "depth"}},
    {"explorer",       "getaddressinfo",                   &GetAddressInfo,                 {"address"}},
    {"explorer",       "getcompactblock",                  &GetCompactBlock,                {"blockHash"}},
    {"explorer",       "getlastblocks",                    &GetLastBlocks,                  {"count", "lastHeight", "verbose"}},
    {"explorer",       "searchbyhash",                     &SearchByHash,                   {"value"}},
    {"explorer",       "gettransactions",                  &GetTransactions,                {"transactions"}},
    {"explorer",       "getaddresstransactions",           &GetAddressTransactions,         {"address"}},
    {"explorer",       "getblocktransactions",             &GetBlockTransactions,           {"blockHash"}},

    // System
    {"system",         "getpeerinfo",                      &GetPeerInfo,                    {}},
    {"system",         "getnodeinfo",                      &GetNodeInfo,                    {}},
    {"system",         "gettime",                          &GetTime,                        {}},
    {"system",         "getcoininfo",                      &GetCoinInfo,                    {"height"}},

    // Transactions
    {"transaction",    "getrawtransaction",                &GetTransaction,                 {"txid"}},
    {"transaction",    "estimatesmartfee",                 &EstimateSmartFee,               {"conf_target", "estimate_mode"} },
};
// @formatter:on

// @formatter:off
static const CRPCCommand commands_post[] =
{
    {"transaction",    "sendrawtransactionwithmessage",    &AddTransaction,                 {"hexstring", "message"}},
    {"transaction",    "addtransaction",                   &AddTransaction,                 {"hexstring", "message"}},
    {"transaction",    "sendrawtransaction",               &AddTransaction,                 {"hexstring", "message"}},
};
// @formatter:on

void RegisterPocketnetWebRPCCommands(CRPCTable &tableRPC, CRPCTable &tablePostRPC)
{
    for (const auto& command : commands)
        tableRPC.appendCommand(command.name, &command);

    for (const auto& command : commands_post)
        tablePostRPC.appendCommand(command.name, &command);
}
