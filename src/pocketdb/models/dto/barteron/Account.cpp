// Copyright (c) 2023 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include <primitives/transaction.h>
#include "pocketdb/models/dto/barteron/Account.h"

namespace PocketTx
{
    BarteronAccount::BarteronAccount() : BarteronList()
    {
        SetType(TxType::BARTERON_ACCOUNT);
    }

    BarteronAccount::BarteronAccount(const CTransactionRef& tx) : BarteronList(tx)
    {
        SetType(TxType::BARTERON_ACCOUNT);
    }

    optional<string> BarteronAccount::GetPayloadTagsAdd() const { return GetPayload() ? GetPayload()->GetString4() : nullopt; }
    const optional<vector<int64_t>> BarteronAccount::GetPayloadTagsAddIds() const { return ParseList(GetPayloadTagsAdd()); }

    optional<string> BarteronAccount::GetPayloadTagsDel() const { return GetPayload() ? GetPayload()->GetString5() : nullopt; }
    const optional<vector<int64_t>> BarteronAccount::GetPayloadTagsDelIds() const { return ParseList(GetPayloadTagsDel()); }

} // namespace PocketTx