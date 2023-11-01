// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/web/PocketTagsRpc.h"

namespace PocketWeb::PocketWebRpc
{
    RPCHelpMan GetTags()
    {
        return RPCHelpMan{
                "gettags",
                "\nReturn N top used tags for language\n",
                // TODO (team): provide description for args, returns and examples
                {
                    
                },
                {

                },
                RPCExamples{
                    ""
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
    {
        if (request.fHelp)
            throw std::runtime_error(
                "gettags\n"
                "\nReturn N top used tags for language\n");

        int pageStart = 0;
        if (request.params.size() > 0)
            ParseInt32(request.params[0].get_str(), &pageStart);

        int pageSize = 50;
        if (request.params.size() > 1)
            ParseInt32(request.params[1].get_str(), &pageSize);

        string lang = "en";
        if (request.params.size() > 3) {
            lang = request.params[3].get_str();
        }

        return request.DbConnection()->WebRpcRepoInst->GetTags(lang, pageSize, pageStart);
    },
        };
    }
}
