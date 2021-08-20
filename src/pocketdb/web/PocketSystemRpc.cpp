// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "PocketSystemRpc.h"

namespace PocketWeb::PocketWebRpc
{

    UniValue GetTime(const JSONRPCRequest& request)
    {
        if (request.fHelp)
            throw std::runtime_error(
                "gettime\n"
                "\nReturn node time.\n");

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("time", GetAdjustedTime());

        return entry;
    }

    UniValue GetPeerInfo(const JSONRPCRequest& request)
    {
        if (request.fHelp)
            throw std::runtime_error(
                "getpeerinfo\n"
                "\nReturns data about each connected network node as a json array of objects.\n"
            );

        UniValue ret(UniValue::VARR);

        std::vector<CNodeStats> vstats;
        g_connman->GetNodeStats(vstats);
        for (const CNodeStats& stats : vstats) {
            UniValue obj(UniValue::VOBJ);

            obj.pushKV("addr", stats.addrName);
            obj.pushKV("services", strprintf("%016x", stats.nServices));
            obj.pushKV("relaytxes", stats.fRelayTxes);
            obj.pushKV("lastsend", stats.nLastSend);
            obj.pushKV("lastrecv", stats.nLastRecv);
            obj.pushKV("conntime", stats.nTimeConnected);
            obj.pushKV("timeoffset", stats.nTimeOffset);
            obj.pushKV("pingtime", stats.dPingTime);
            obj.pushKV("protocol", stats.nVersion);
            obj.pushKV("version", stats.cleanSubVer);
            obj.pushKV("inbound", stats.fInbound);
            obj.pushKV("addnode", stats.m_manual_connection);
            obj.pushKV("startingheight", stats.nStartingHeight);
            obj.pushKV("whitelisted", stats.fWhitelisted);

            ret.push_back(obj);
        }

        return ret;
    }

    UniValue GetNodeInfo(const JSONRPCRequest& request)
    {
        if (request.fHelp)
            throw std::runtime_error(
                "getnodeinfo\n"
                "\nReturns data about node.\n"
            );

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("version", FormatVersion(CLIENT_VERSION));
        entry.pushKV("time", GetAdjustedTime());
        entry.pushKV("chain", Params().NetworkIDString());
        entry.pushKV("proxy", true);

        uint64_t nNetworkWeight = GetPoSKernelPS();
        entry.pushKV("netstakeweight", (uint64_t)nNetworkWeight);

        CBlockIndex* pindex = chainActive.Tip();
        UniValue oblock(UniValue::VOBJ);
        oblock.pushKV("height", pindex->nHeight);
        oblock.pushKV("hash", pindex->GetBlockHash().GetHex());
        oblock.pushKV("time", (int64_t)pindex->nTime);
        oblock.pushKV("ntx", (int)pindex->nTx);
        entry.pushKV("lastblock", oblock);

        if (!WSConnections.empty()) {
            UniValue proxies(UniValue::VARR);
            for (auto& it : WSConnections) {
                if (it.second.Service) {
                    UniValue proxy(UniValue::VOBJ);
                    proxy.pushKV("address", it.second.Address);
                    proxy.pushKV("ip", it.second.Ip);
                    proxy.pushKV("port", it.second.MainPort);
                    proxy.pushKV("portWss", it.second.WssPort);
                    proxies.push_back(proxy);
                }
            }
            entry.pushKV("proxies", proxies);
        }

        return entry;
    }

}