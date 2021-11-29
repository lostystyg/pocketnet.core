// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef SRC_SEARCH_RPC_H
#define SRC_SEARCH_RPC_H

#include "rpc/server.h"
#include "util/html.h"
#include "pocketdb/helpers/TransactionHelper.h"
#include "pocketdb/models/base/PocketTypes.h"
#include "pocketdb/models/web/SearchRequest.h"
#include "pocketdb/web/WebRpcUtils.h"

namespace PocketWeb::PocketWebRpc
{
    using namespace PocketDbWeb;
    using namespace PocketTx;

    RPCHelpMan Search();
    RPCHelpMan SearchUsers();
    RPCHelpMan SearchLinks();
}

#endif //SRC_SEARCH_RPC_H
