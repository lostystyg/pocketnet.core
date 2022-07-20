// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/repositories/CheckpointRepository.h"

namespace PocketDb
{
    bool CheckpointRepository::IsSocialCheckpoint(const string& txHash, TxType txType, int code)
    {
        if (_socialCheckpoints.find(txHash) != _socialCheckpoints.end())
            if (auto[t, c] = _socialCheckpoints[txHash]; t == txType && c == code)
                return true;

        return false;
    }

    bool CheckpointRepository::IsLotteryCheckpoint(int height, const string& hash)
    {
        if (_lotteryCheckpoints.find(height) != _lotteryCheckpoints.end())
            if (_lotteryCheckpoints[height] == hash)
                return true;

        return false;
    }

    bool CheckpointRepository::IsOpReturnCheckpoint(const string& txHash, const string& hash)
    {
        if (_opReturnCheckpoints.find(txHash) != _opReturnCheckpoints.end())
            if (_opReturnCheckpoints[txHash] == hash)
                return true;

        return false;
    }
}
