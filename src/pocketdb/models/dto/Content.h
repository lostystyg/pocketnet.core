// Copyright (c) 2018-2022 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETTX_CONTENT_H
#define POCKETTX_CONTENT_H

#include "pocketdb/models/base/Transaction.h"

namespace PocketTx
{
    using namespace std;

    class Content : public Transaction
    {
    public:
        Content();
        Content(const CTransactionRef& tx);

        shared_ptr<UniValue> Serialize() const override;

        void Deserialize(const UniValue& src) override;
        void DeserializeRpc(const UniValue& src) override;
        void DeserializePayload(const UniValue& src) override;

        shared_ptr<string> GetAddress() const;
        void SetAddress(const string& value) override;

        shared_ptr<string> GetRootTxHash() const;
        void SetRootTxHash(const string& value);

        shared_ptr<string> GetRelayTxHash() const;
        void SetRelayTxHash(const string& value);

        bool IsEdit() const;

        shared_ptr<string> GetPayloadLang() const;
        shared_ptr<string> GetPayloadCaption() const;
        shared_ptr<string> GetPayloadMessage() const;
        shared_ptr<string> GetPayloadTags() const;
        shared_ptr<string> GetPayloadUrl() const;
        shared_ptr<string> GetPayloadImages() const;
        shared_ptr<string> GetPayloadSettings() const;

        string BuildHash() override;

    };

} // namespace PocketTx

#endif // POCKETTX_CONTENT_H