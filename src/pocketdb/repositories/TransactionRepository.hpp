// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 Bitcoin developers
// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETDB_TRANSACTIONREPOSITORY_HPP
#define POCKETDB_TRANSACTIONREPOSITORY_HPP

#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include "pocketdb/helpers/TransactionHelper.h"
#include "pocketdb/repositories/BaseRepository.hpp"
#include "pocketdb/models/base/Transaction.h"
#include "pocketdb/models/base/TransactionOutput.h"
#include "pocketdb/models/base/Rating.h"
#include "pocketdb/models/base/ReturnDtoModels.h"
#include "pocketdb/models/dto/User.h"
#include "pocketdb/models/dto/ScoreContent.h"
#include "pocketdb/models/dto/ScoreComment.h"

namespace PocketDb
{
    using std::runtime_error;
    using boost::algorithm::join;
    using boost::adaptors::transformed;

    using namespace PocketTx;
    using namespace PocketHelpers;

    class TransactionRepository : public BaseRepository
    {
    public:
        explicit TransactionRepository(SQLiteDatabase& db) : BaseRepository(db) {}

        void Init() override {}
        void Destroy() override {}

        //  Base transaction operations
        void InsertTransactions(PocketBlock& pocketBlock)
        {
            TryTransactionStep(__func__, [&]()
            {
                for (const auto& ptx : pocketBlock)
                {
                    // Insert general transaction
                    InsertTransactionModel(ptx);

                    // Outputs
                    InsertTransactionOutputs(ptx);

                    // Also need insert payload of transaction
                    // But need get new rowId
                    // If last id equal 0 - insert ignored - or already exists or error -> paylod not inserted
                    if (ptx->HasPayload())
                        InsertTransactionPayload(ptx);
                }
            });
        }

        shared_ptr<PocketBlock> GetList(const vector<string>& txHashes, bool includePayload = false)
        {
            auto sql = R"sql(
                SELECT
                    t.Type,
                    t.Hash,
                    t.Time,
                    t.Last,
                    t.Id,
                    t.String1,
                    t.String2,
                    t.String3,
                    t.String4,
                    t.String5,
                    t.Int1,
                    p.TxHash pHash,
                    p.String1 pString1,
                    p.String2 pString2,
                    p.String3 pString3,
                    p.String4 pString4,
                    p.String5 pString5,
                    p.String6 pString6,
                    p.String7 pString7
                FROM Transactions t
                LEFT JOIN Payload p on t.Hash = p.TxHash
                WHERE t.Hash in ( )sql" + join(vector<string>(txHashes.size(), "?"), ",") + R"sql( )
            )sql";

            auto result = make_shared<PocketBlock>(PocketBlock{});
            TryTransactionStep(__func__, [&]()
            {
                auto stmt = SetupSqlStatement(sql);

                for (size_t i = 0; i < txHashes.size(); i++)
                    TryBindStatementText(stmt, (int)i + 1, txHashes[i]);

                while (sqlite3_step(*stmt) == SQLITE_ROW)
                {
                    if (auto[ok, transaction] = CreateTransactionFromListRow(stmt, includePayload); ok)
                        result->push_back(transaction);
                }

                FinalizeSqlStatement(*stmt);
            });

            return result;
        }

        shared_ptr<Transaction> GetByHash(const string& hash, bool includePayload = false)
        {
            auto lst = GetList({hash}, includePayload);
            if (!lst->empty())
                return lst->front();

            return nullptr;
        }

        bool ExistsByHash(const string& hash)
        {
            bool result = false;

            TryTransactionStep(__func__, [&]()
            {
                auto stmt = SetupSqlStatement(R"sql(
                    SELECT count(*)
                    FROM Transactions
                    WHERE Hash = ?
                )sql");

                TryBindStatementText(stmt, 1, hash);

                if (sqlite3_step(*stmt) == SQLITE_ROW)
                    if (auto[ok, value] = TryGetColumnInt(*stmt, 0); ok)
                        result = (value >= 1);

                FinalizeSqlStatement(*stmt);
            });

            return result;
        }

    private:

        void InsertTransactionOutputs(const PTransactionRef& ptx)
        {
            for (const auto& output : ptx->Outputs())
            {
                // Build transaction output
                auto stmt = SetupSqlStatement(R"sql(
                    INSERT OR FAIL INTO TxOutputs (
                        TxHash,
                        Number,
                        AddressHash,
                        Value,
                        ScriptPubKey
                    ) SELECT ?,?,?,?,?
                    WHERE NOT EXISTS (
                        select 1
                        from TxOutputs o
                        where o.TxHash = ?
                            and o.Number = ?
                            and o.AddressHash = ?
                    )
                )sql");

                TryBindStatementText(stmt, 1, ptx->GetHash());
                TryBindStatementInt64(stmt, 2, output->GetNumber());
                TryBindStatementText(stmt, 3, output->GetAddressHash());
                TryBindStatementInt64(stmt, 4, output->GetValue());
                TryBindStatementText(stmt, 5, output->GetScriptPubKey());
                TryBindStatementText(stmt, 6, ptx->GetHash());
                TryBindStatementInt64(stmt, 7, output->GetNumber());
                TryBindStatementText(stmt, 8, output->GetAddressHash());

                TryStepStatement(stmt);
            }
        }

        void InsertTransactionPayload(const PTransactionRef& ptx)
        {
            auto stmt = SetupSqlStatement(R"sql(
                INSERT OR FAIL INTO Payload (
                    TxHash,
                    String1,
                    String2,
                    String3,
                    String4,
                    String5,
                    String6,
                    String7
                ) SELECT
                    ?,?,?,?,?,?,?,?
                WHERE not exists (select 1 from Payload p where p.TxHash = ?)
            )sql");

            TryBindStatementText(stmt, 1, ptx->GetHash());
            TryBindStatementText(stmt, 2, ptx->GetPayload()->GetString1());
            TryBindStatementText(stmt, 3, ptx->GetPayload()->GetString2());
            TryBindStatementText(stmt, 4, ptx->GetPayload()->GetString3());
            TryBindStatementText(stmt, 5, ptx->GetPayload()->GetString4());
            TryBindStatementText(stmt, 6, ptx->GetPayload()->GetString5());
            TryBindStatementText(stmt, 7, ptx->GetPayload()->GetString6());
            TryBindStatementText(stmt, 8, ptx->GetPayload()->GetString7());
            TryBindStatementText(stmt, 9, ptx->GetHash());

            TryStepStatement(stmt);
        }

        void InsertTransactionModel(const PTransactionRef& ptx)
        {
            auto stmt = SetupSqlStatement(R"sql(
                INSERT OR FAIL INTO Transactions (
                    Type,
                    Hash,
                    Time,
                    String1,
                    String2,
                    String3,
                    String4,
                    String5,
                    Int1
                ) SELECT ?,?,?,?,?,?,?,?,?
                WHERE not exists (select 1 from Transactions t where t.Hash=?)
            )sql");

            TryBindStatementInt(stmt, 1, ptx->GetTypeInt());
            TryBindStatementText(stmt, 2, ptx->GetHash());
            TryBindStatementInt64(stmt, 3, ptx->GetTime());
            TryBindStatementText(stmt, 4, ptx->GetString1());
            TryBindStatementText(stmt, 5, ptx->GetString2());
            TryBindStatementText(stmt, 6, ptx->GetString3());
            TryBindStatementText(stmt, 7, ptx->GetString4());
            TryBindStatementText(stmt, 8, ptx->GetString5());
            TryBindStatementInt64(stmt, 9, ptx->GetInt1());
            TryBindStatementText(stmt, 10, ptx->GetHash());

            TryStepStatement(stmt);
        }

    protected:

        tuple<bool, PTransactionRef> CreateTransactionFromListRow(const shared_ptr<sqlite3_stmt*>& stmt, bool includedPayload)
        {
            auto[ok0, txType] = TryGetColumnInt(*stmt, 0);
            auto[ok1, txHash] = TryGetColumnString(*stmt, 1);
            auto[ok2, nTime] = TryGetColumnInt64(*stmt, 2);

            if (!ok0 || !ok1 || !ok2)
                return make_tuple(false, nullptr);

            auto ptx = PocketHelpers::TransactionHelper::CreateInstance(static_cast<PocketTxType>(txType));
            ptx->SetTime(nTime);
            ptx->SetHash(txHash);

            if (ptx == nullptr)
                return make_tuple(false, nullptr);

            if (auto[ok, value] = TryGetColumnInt(*stmt, 3); ok) ptx->SetLast(value == 1);
            if (auto[ok, value] = TryGetColumnInt64(*stmt, 4); ok) ptx->SetId(value);
            if (auto[ok, value] = TryGetColumnString(*stmt, 5); ok) ptx->SetString1(value);
            if (auto[ok, value] = TryGetColumnString(*stmt, 6); ok) ptx->SetString2(value);
            if (auto[ok, value] = TryGetColumnString(*stmt, 7); ok) ptx->SetString3(value);
            if (auto[ok, value] = TryGetColumnString(*stmt, 8); ok) ptx->SetString4(value);
            if (auto[ok, value] = TryGetColumnString(*stmt, 9); ok) ptx->SetString5(value);
            if (auto[ok, value] = TryGetColumnInt64(*stmt, 10); ok) ptx->SetInt1(value);

            if (!includedPayload)
                return make_tuple(true, ptx);

            if (auto[ok, value] = TryGetColumnString(*stmt, 11); !ok)
                return make_tuple(true, ptx);

            auto payload = Payload();
            if (auto[ok, value] = TryGetColumnString(*stmt, 11); ok) payload.SetTxHash(value);
            if (auto[ok, value] = TryGetColumnString(*stmt, 12); ok) payload.SetString1(value);
            if (auto[ok, value] = TryGetColumnString(*stmt, 13); ok) payload.SetString2(value);
            if (auto[ok, value] = TryGetColumnString(*stmt, 14); ok) payload.SetString3(value);
            if (auto[ok, value] = TryGetColumnString(*stmt, 15); ok) payload.SetString4(value);
            if (auto[ok, value] = TryGetColumnString(*stmt, 16); ok) payload.SetString5(value);
            if (auto[ok, value] = TryGetColumnString(*stmt, 17); ok) payload.SetString6(value);
            if (auto[ok, value] = TryGetColumnString(*stmt, 18); ok) payload.SetString7(value);

            ptx->SetPayload(payload);

            return make_tuple(true, ptx);
        }

    };
} // namespace PocketDb

#endif // POCKETDB_TRANSACTIONREPOSITORY_HPP

