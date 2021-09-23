// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETTX_TRANSACTION_H
#define POCKETTX_TRANSACTION_H

#include <string>
#include <univalue/include/univalue.h>
#include <utility>
#include <utilstrencodings.h>
#include <crypto/sha256.h>

#include "pocketdb/models/base/Payload.h"
#include "pocketdb/models/base/TransactionOutput.h"

namespace PocketTx
{
    using namespace std;

    class Transaction : public Base
    {
    public:
        Transaction();

        explicit Transaction(const std::shared_ptr<const CTransaction>& tx);

        virtual shared_ptr<UniValue> Serialize() const;

        virtual void Deserialize(const UniValue& src);
        virtual void DeserializeRpc(const UniValue& src, const std::shared_ptr<const CTransaction>& tx);
        virtual void DeserializePayload(const UniValue& src, const std::shared_ptr<const CTransaction>& tx);

        virtual void BuildHash() = 0;

        shared_ptr<string> GetHash() const;
        void SetHash(string value);
        bool operator==(const string& hash) const;

        shared_ptr<PocketTxType> GetType() const;
        shared_ptr<int> GetTypeInt() const;
        void SetType(PocketTxType value);

        shared_ptr<int64_t> GetTime() const;
        void SetTime(int64_t value);

        shared_ptr<bool> GetLast() const;
        void SetLast(bool value);

        shared_ptr<string> GetString1() const;
        void SetString1(string value);

        shared_ptr<string> GetString2() const;
        void SetString2(string value);

        shared_ptr<string> GetString3() const;
        void SetString3(string value);

        shared_ptr<string> GetString4() const;
        void SetString4(string value);

        shared_ptr<string> GetString5() const;
        void SetString5(string value);

        shared_ptr<int64_t> GetInt1() const;
        void SetInt1(int64_t value);

        shared_ptr<int64_t> GetId() const;
        void SetId(int64_t value);

        vector<shared_ptr<TransactionOutput>>& Outputs();

        shared_ptr<Payload> GetPayload() const;
        void SetPayload(Payload value);
        bool HasPayload() const;

    protected:
        shared_ptr<PocketTxType> m_type = nullptr;
        shared_ptr<string> m_hash = nullptr;
        shared_ptr<int64_t> m_time = nullptr;
        shared_ptr<int64_t> m_id = nullptr;
        shared_ptr<bool> m_last = nullptr;
        shared_ptr<string> m_string1 = nullptr;
        shared_ptr<string> m_string2 = nullptr;
        shared_ptr<string> m_string3 = nullptr;
        shared_ptr<string> m_string4 = nullptr;
        shared_ptr<string> m_string5 = nullptr;
        shared_ptr<int64_t> m_int1 = nullptr;
        shared_ptr<Payload> m_payload = nullptr;
        vector<shared_ptr<TransactionOutput>> m_outputs;

        string GenerateHash(const string& data) const;
        void GeneratePayload();
        void ClearPayload();
    };

} // namespace PocketTx

#endif // POCKETTX_TRANSACTION_H