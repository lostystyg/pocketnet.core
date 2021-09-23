// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETTX_COINSTAKE_H
#define POCKETTX_COINSTAKE_H

#include "pocketdb/models/base/Transaction.h"

namespace PocketTx
{
    class Coinstake : public Transaction
    {
    public:
        Coinstake();
        Coinstake(const std::shared_ptr<const CTransaction>& tx);

        void Deserialize(const UniValue& src) override;
    protected:
        void DeserializePayload(const UniValue& src, const std::shared_ptr<const CTransaction>& tx) override;
        void BuildHash() override;
    };
} // namespace PocketTx

#endif // POCKETTX_COINSTAKE_H
