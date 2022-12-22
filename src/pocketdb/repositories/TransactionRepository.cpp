// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/repositories/TransactionRepository.h"

namespace PocketDb
{
    class CollectDataToModelConverter
    {
    public:
        // TODO (losty-db): consider remove `repository` parameter from here.
        static optional<CollectData> ModelToCollectData(const PTransactionRef& ptx, TransactionRepository& repository)
        {
            if (!ptx->GetHash()) return nullopt;
            TxContextualData txData;
            optional<CollectData> res;
            if (!DbViewHelper::Extract(txData, ptx)) {
                return nullopt;
            }

            auto txId = repository.TxHashToId(*ptx->GetHash());
            if (!txId) return nullopt;

            CollectData collectData(*txId, *ptx->GetHash());
            collectData.txContextData = std::move(txData);
            collectData.inputs = ptx->Inputs();
            collectData.outputs = ptx->OutputsConst();
            collectData.payload = ptx->GetPayload();
            collectData.ptx = ptx;

            return collectData;
        }

        static PTransactionRef CollectDataToModel(const CollectData& collectData)
        {
            auto ptx = collectData.ptx;

            if (!DbViewHelper::Inject(ptx, collectData.txContextData))
                return nullptr;

            ptx->Inputs() = collectData.inputs;
            ptx->Outputs() = collectData.outputs;
            if (collectData.payload) ptx->SetPayload(*collectData.payload);
            return ptx;
        }
    };

    class TransactionReconstructor : public RowAccessor
    {
    public:
        TransactionReconstructor(std::map<int64_t, CollectData> initData) {
            m_collectData = std::move(initData);
        };

        /**
         * Pass a new row for reconstructor to collect all necessary data from it.
         * @param stmt - sqlite stmt. Null is not allowed and will potentially cause a segfault
         * Returns boolean result of collecting data. If false is returned - all data inside reconstructor is possibly corrupted due to bad input (missing columns, etc)
         * and it should not be used anymore
         * 
         * Columns: PartType (0), TxHash (1), ...
         * PartTypes: 0 Tx, 1 Payload, 2 Input, 3 Output
         */
        bool FeedRow(sqlite3_stmt* stmt)
        {
            auto[okPartType, partType] = TryGetColumnInt(stmt, 0);
            if (!okPartType) return false;

            auto[okTxId, txId] = TryGetColumnInt64(stmt, 1);
            if (!okTxId) return false;

            auto dataItr = m_collectData.find(txId);
            if (dataItr == m_collectData.end()) return false;

            auto& data = dataItr->second;

            switch (partType)
            {
                case 0: {
                    return ParseTransaction(stmt, data);
                }
                case 1:
                    return ParsePayload(stmt, data);
                case 2:
                    return ParseInput(stmt, data);
                case 3:
                    return ParseOutput(stmt, data);
                default:
                    return false;
            }
        }

        /**
         * Return contructed block.
         */
        std::vector<CollectData> GetResult()
        {
            std::vector<CollectData> res;

            transform(
                m_collectData.begin(),
                m_collectData.end(),
                back_inserter(res),
                [](const auto& elem) {return elem.second;});

            return res;
        }

    private:

        map<int64_t, CollectData> m_collectData;

        /**
         * This method parses transaction data, constructs if needed and return construct entry to fill
         * Index:   0  1     2     3     4          5       6     7   8        9        10       11       12       13    14    15
         * Columns: 0, Hash, Type, Time, BlockHash, Height, Last, Id, String1, String2, String3, String4, String5, null, null, Int1
         */
        bool ParseTransaction(sqlite3_stmt* stmt, CollectData& collectData)
        {
            // Try get Type and create pocket transaction instance
            auto[okType, txType] = TryGetColumnInt(stmt, 2);
            if (!okType) return false;
            
            PTransactionRef ptx = PocketHelpers::TransactionHelper::CreateInstance(static_cast<TxType>(txType));
            if (!ptx) return false;

            if (auto[ok, value] = TryGetColumnInt64(stmt, 3); ok) ptx->SetTime(value);
            if (auto[ok, value] = TryGetColumnInt64(stmt, 4); ok) ptx->SetHeight(value);
            // TODO (losty-db): implement "first" field
            // if (auto[ok, value] = TryGetColumnInt64(stmt, 5); ok) ptx->SetFirst(value);
            if (auto[ok, value] = TryGetColumnInt64(stmt, 6); ok) ptx->SetLast(value);
            if (auto[ok, value] = TryGetColumnInt64(stmt, 7); ok) ptx->SetId(value);
            if (auto[ok, value] = TryGetColumnInt64(stmt, 8); ok) ptx->SetInt1(value);
            if (auto[ok, value] = TryGetColumnString(stmt, 9); ok) ptx->SetString1(value);
            if (auto[ok, value] = TryGetColumnString(stmt, 10); ok) ptx->SetString2(value);
            if (auto[ok, value] = TryGetColumnString(stmt, 11); ok) ptx->SetString3(value);
            if (auto[ok, value] = TryGetColumnString(stmt, 12); ok) ptx->SetString4(value);
            if (auto[ok, value] = TryGetColumnString(stmt, 13); ok) ptx->SetString5(value);
            if (auto[ok, value] = TryGetColumnString(stmt, 14); ok) ptx->SetBlockHash(value);

            collectData.ptx = std::move(ptx);

            PocketHelpers::TxContextualData txContextData;
            if (auto[ok, value] = TryGetColumnString(stmt, 15); ok) txContextData.list = value;
            
            collectData.txContextData = std::move(txContextData);

            return true;
        }

        /**
         * Parse Payload
         * Index:   0  1       2     3     4     5     6     7     8        9        10       11       12       13       14       15
         * Columns: 1, TxHash, null, null, null, null, null, null, String1, String2, String3, String4, String5, String6, String7, Int1
         */
        bool ParsePayload(sqlite3_stmt* stmt, CollectData& collectData)
        {
            Payload payload;
            if (auto[ok, value] = TryGetColumnInt64(stmt, 2); ok) { payload.SetInt1(value); }
            if (auto[ok, value] = TryGetColumnString(stmt, 8); ok) { payload.SetString1(value); }
            if (auto[ok, value] = TryGetColumnString(stmt, 9); ok) { payload.SetString2(value); }
            if (auto[ok, value] = TryGetColumnString(stmt, 10); ok) { payload.SetString3(value); }
            if (auto[ok, value] = TryGetColumnString(stmt, 11); ok) { payload.SetString4(value); }
            if (auto[ok, value] = TryGetColumnString(stmt, 12); ok) { payload.SetString5(value); }
            if (auto[ok, value] = TryGetColumnString(stmt, 13); ok) { payload.SetString6(value); }
            if (auto[ok, value] = TryGetColumnString(stmt, 14); ok) { payload.SetString7(value); }

            collectData.payload = std::move(payload);

            return true;
        }

        /**
         * Parse Inputs
         * Index:   0  1              2     3     4         5         6        7     8              9     10    11    12    13    14    15
         * Columns: 2, i.SpentTxHash, null, null, i.TxHash, i.Number, o.Value, null, o.AddressHash, null, null, null, null, null, null, null
         */
        bool ParseInput(sqlite3_stmt* stmt, CollectData& collectData)
        {
            bool incomplete = false;

            TransactionInput input;
            input.SetSpentTxHash(collectData.txHash);
            if (auto[ok, value] = TryGetColumnInt64(stmt, 2); ok) input.SetNumber(value);
            else incomplete = true;

            if (auto[ok, value] = TryGetColumnInt64(stmt, 3); ok) input.SetValue(value);

            if (auto[ok, value] = TryGetColumnString(stmt, 8); ok) input.SetTxHash(value);
            else incomplete = true;

            if (auto[ok, value] = TryGetColumnString(stmt, 9); ok) input.SetAddressHash(value);

            collectData.inputs.emplace_back(input);
            return !incomplete;
        }

        /**
         * Parse Outputs
         * Index:   0  1       2     3       4            5      6     7     8     9     10            11    12    13    14           15
         * Columns: 3, TxHash, null, Number, AddressHash, Value, null, null, null, null, ScriptPubKey, null, null, null, SpentTxHash, SpentHeight
         */
        bool ParseOutput(sqlite3_stmt* stmt, CollectData& collectData)
        {
            bool incomplete = false;

            TransactionOutput output;
            output.SetTxHash(collectData.txHash);

            if (auto[ok, value] = TryGetColumnInt64(stmt, 2); ok) output.SetValue(value);
            else incomplete = true;

            if (auto[ok, value] = TryGetColumnInt64(stmt, 3); ok) output.SetNumber(value);
            else incomplete = true;

            if (auto[ok, value] = TryGetColumnString(stmt, 8); ok) output.SetAddressHash(value);
            else incomplete = true;

            if (auto[ok, value] = TryGetColumnString(stmt, 9); ok) output.SetScriptPubKey(value);
            else incomplete = true;

            collectData.outputs.emplace_back(output);
            return !incomplete;
        }
    };

    void TransactionRepository::InsertTransactions(PocketBlock& pocketBlock)
    {
        TryTransactionStep(__func__, [&]()
        {
            for (const auto& ptx: pocketBlock)
            {
                auto collectData = CollectDataToModelConverter::ModelToCollectData(ptx, *this);
                if (!collectData) {
                    // TODO (losty-db): error!!!
                    LogPrintf("DEBUG: failed to convert model to CollectData in %s\n", __func__);
                    continue;
                }
                // Insert general transaction
                InsertTransactionModel(*collectData);

                // Inputs
                InsertTransactionInputs(collectData->inputs, collectData->txId);

                // Outputs
                InsertTransactionOutputs(collectData->outputs, collectData->txId);

                // Also need insert payload of transaction
                // But need get new rowId
                // If last id equal 0 - insert ignored - or already exists or error -> paylod not inserted
                if (collectData->payload)
                    InsertTransactionPayload(*collectData->payload);
            }
        });
    }

    PocketBlockRef TransactionRepository::List(const vector<string>& txHashes, bool includePayload, bool includeInputs, bool includeOutputs)
    {
        string txReplacers = join(vector<string>(txHashes.size(), "?"), ",");
        auto txIdMap = GetTxIds(txHashes);

        if (txIdMap.size() != txHashes.size()) {
            // TODO (losty-db): error
            LogPrintf("DEBUG: missing ids for requested hashses\n");
        }

        auto sql = R"sql(
            select (0)tp, t.Type, t.Time, c.Height, c.First, c.Last, c.Id, t.Int1 
                (select r.String from Registry r where r.Id = t.RegId1),
                (select r.String from Registry r where r.Id = t.RegId2),
                (select r.String from Registry r where r.Id = t.RegId3),
                (select r.String from Registry r where r.Id = t.RegId4)
                (select r.String from Registry r where r.Id = t.RegId5)
                (select r.String from Registry r where r.Id = c.BlockId),
                (
                    -- TODO (losty-db): empty string instead of empty array
                    select json_group_array(
                        (select rr.String from Registry rr where rr.Id = l.RegId)
                    )
                    from Lists l
                    where l.TxId = t.Id
                )
            from Transactions t
            left join Chain c
                on c.TxId = t.Id
            where t.Id in ( )sql" + txReplacers + R"sql( )
        )sql" +

        // Payload part
        (includePayload ? string(R"sql(
            union
            select (1)tp, TxId, Int1, null, null, null, null, null, String1, String2, String3, String4, String5, String6, String7
            from Payload
            where TxId in ( )sql" + txReplacers + R"sql( )
        )sql") : "") +

        // Inputs part
        (includeInputs ? string(R"sql(
            union
            select (2)tp, i.SpentTxId, i.Number, o.Value, null, null, null, null,
                (select r.String from Registry r where r.Id = i.TxId),
                (select a.String from Registry a where a.Id = o.AddressId),
                null, null, null, null, null
            from TxInputs i
            join TxOutputs o on o.TxId = i.TxId and o.Number = i.Number
            where i.SpentTxId in ( )sql" + txReplacers + R"sql( )
        )sql") : "") +
        
        // Outputs part
        (includeOutputs ? string(R"sql(
            union
            select (2)tp, TxId, Value, Number, null, null, null, null,(select a.String from Registry a where a.Id = AddressId), ScriptPubKey, null, null, null
            from TxOutputs
            where TxId in ( )sql" + txReplacers + R"sql( )
        )sql") : "") +

        string(R"sql(
            order by tp asc
        )sql");

        std::map<int64_t, CollectData> initData;
        for (const auto& [hash, txId]: txIdMap) {
            initData.emplace(txId, CollectData{txId,hash});
        }
        
        TransactionReconstructor reconstructor(initData);

        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);
            size_t i = 1;

            for (const auto& [hash, txId] : txIdMap)
                TryBindStatementInt64(stmt, i++, txId);
            
            if (includePayload)
                for (const auto& [hash, txId] : txIdMap)
                    TryBindStatementInt64(stmt, i++, txId);

            if (includeInputs)
                for (const auto& [hash, txId] : txIdMap)
                    TryBindStatementInt64(stmt, i++, txId);

            if (includeOutputs)
                for (const auto& [hash, txId] : txIdMap)
                    TryBindStatementInt64(stmt, i++, txId);

            while (sqlite3_step(*stmt) == SQLITE_ROW)
            {
                // TODO (aok): maybe throw exception if errors?
                if (!reconstructor.FeedRow(*stmt))
                {
                    break;
                }
            }

            FinalizeSqlStatement(*stmt);
        });

        auto pBlock = std::make_shared<PocketBlock>();

        for (auto& collectData: reconstructor.GetResult()) {
            if (auto ptx = CollectDataToModelConverter::CollectDataToModel(collectData); ptx)
                pBlock->emplace_back(ptx);
            else {
                // TODO (losty-db): error!!!
                LogPrintf("DEBUG: failed to get model from CollectData in %s\n" , __func__);
            }
        }

        return pBlock;
    }

    PTransactionRef TransactionRepository::Get(const string& hash, bool includePayload, bool includeInputs, bool includeOutputs)
    {
        auto lst = List({ hash }, includePayload, includeInputs, includeOutputs);
        if (lst && !lst->empty())
            return lst->front();

        return nullptr;
    }

    PTransactionOutputRef TransactionRepository::GetTxOutput(const string& txHash, int number)
    {
        auto txIdMap = GetTxIds({txHash});
        auto txIdItr = txIdMap.find(txHash);

        if (txIdItr == txIdMap.end()) {
            return nullptr;
        }

        auto& txId = txIdItr->second;

        auto sql = R"sql(
            SELECT
                Number,
                (select Hash from Address where Id = AddressId),
                Value,
                ScriptPubKey
            FROM TxOutputs
            WHERE TxId = ?
              and Number = ?
        )sql";

        shared_ptr<TransactionOutput> result = nullptr;
        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);

            TryBindStatementInt64(stmt, 1, txId);
            TryBindStatementInt(stmt, 2, number);

            if (sqlite3_step(*stmt) == SQLITE_ROW)
            {
                result = make_shared<TransactionOutput>();
                result->SetTxHash(txHash);
                if (auto[ok, value] = TryGetColumnInt64(*stmt, 0); ok) result->SetNumber(value);
                if (auto[ok, value] = TryGetColumnString(*stmt, 1); ok) result->SetAddressHash(value);
                if (auto[ok, value] = TryGetColumnInt64(*stmt, 2); ok) result->SetValue(value);
                if (auto[ok, value] = TryGetColumnString(*stmt, 3); ok) result->SetScriptPubKey(value);
            }

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    bool TransactionRepository::Exists(const string& hash)
    {
        bool result = false;

        string sql = R"sql(
            select 1
            from Transactions
            where Hash = ?
        )sql";

        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);

            TryBindStatementText(stmt, 1, hash);

            result = (sqlite3_step(*stmt) == SQLITE_ROW);

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    bool TransactionRepository::ExistsInChain(const string& hash)
    {
        bool result = false;

        string sql = R"sql(
            select 1
            from Transactions
            where Hash = ?
              and Height is not null
        )sql";

        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);

            TryBindStatementText(stmt, 1, hash);

            if (sqlite3_step(*stmt) == SQLITE_ROW)
                result = true;

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    int TransactionRepository::MempoolCount()
    {
        int result = 0;

        string sql = R"sql(
            select count(*)
            from Transactions
            where Height isnull
              and Type != 3
        )sql";

        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);

            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, value] = TryGetColumnInt(*stmt, 0); ok)
                    result = value;

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    void TransactionRepository::Clean()
    {
        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(R"sql(
                delete from Transactions
                where Height is null
            )sql");

            TryStepStatement(stmt);
        });
    }

    void TransactionRepository::CleanTransaction(const string& hash)
    {
        TryTransactionStep(__func__, [&]()
        {
            // Clear Payload table
            auto stmt = SetupSqlStatement(R"sql(
                delete from Payload
                where TxHash = ?
                  and exists(
                    select 1
                    from Transactions t
                    where t.Hash = Payload.TxHash
                      and t.Height isnull
                  )
            )sql");
            TryBindStatementText(stmt, 1, hash);
            TryStepStatement(stmt);

            // Clear TxOutputs table
            stmt = SetupSqlStatement(R"sql(
                delete from TxOutputs
                where TxHash = ?
                  and exists(
                    select 1
                    from Transactions t
                    where t.Hash = TxOutputs.TxHash
                      and t.Height isnull
                  )
            )sql");
            TryBindStatementText(stmt, 1, hash);
            TryStepStatement(stmt);

            // Clear Transactions table
            stmt = SetupSqlStatement(R"sql(
                delete from Transactions
                where Hash = ?
                  and Height isnull
            )sql");
            TryBindStatementText(stmt, 1, hash);
            TryStepStatement(stmt);
        });
    }

    void TransactionRepository::CleanMempool()
    {
        TryTransactionStep(__func__, [&]()
        {
            // Clear Payload table
            auto stmt = SetupSqlStatement(R"sql(
                delete from Payload
                where TxHash in (
                  select t.Hash
                  from Transactions t
                  where t.Height is null
                )
            )sql");
            TryStepStatement(stmt);

            // Clear TxOutputs table
            stmt = SetupSqlStatement(R"sql(
                delete from TxOutputs
                where TxHash in (
                  select t.Hash
                  from Transactions t
                  where t.Height is null
                )
            )sql");
            TryStepStatement(stmt);

            // Clear Transactions table
            stmt = SetupSqlStatement(R"sql(
                delete from Transactions
                where Height isnull
            )sql");
            TryStepStatement(stmt);
        });
    }

    void TransactionRepository::InsertTransactionInputs(const vector<TransactionInput>& inputs, int64_t txId)
    {
        for (const auto& input: inputs)
        {
            // Build transaction output
            auto stmt = SetupSqlStatement(R"sql(
                INSERT OR IGNORE INTO TxInputs
                (
                    SpentTxHash,
                    TxHash,
                    Number
                )
                VALUES
                (
                    (select Id from Transactions where Hash = ?),
                    ?,
                    ?
                )
            )sql");

            TryBindStatementText(stmt, 1, input.GetSpentTxHash());
            TryBindStatementInt64(stmt, 2, txId);
            TryBindStatementInt64(stmt, 3, input.GetNumber());

            TryStepStatement(stmt);
        }
    }
    
    void TransactionRepository::InsertTransactionOutputs(const vector<TransactionOutput>& outputs, int64_t txId)
    {
        for (const auto& output: outputs)
        {
            // Build transaction output
            auto stmt = SetupSqlStatement(R"sql(
                INSERT OR IGNORE INTO TxOutputs (
                    TxId,
                    Number,
                    AddressId,
                    Value,
                    ScriptPubKey
                )
                VALUES
                (
                    ?,
                    ?,
                    (select Id from Addresses where Hash = ?),
                    ?,
                    ?
                )
            )sql");

            TryBindStatementInt64(stmt, 1, txId);
            TryBindStatementInt64(stmt, 2, output.GetNumber());
            TryBindStatementText(stmt, 3, output.GetAddressHash());
            TryBindStatementInt64(stmt, 4, output.GetValue());
            TryBindStatementText(stmt, 5, output.GetScriptPubKey());

            TryStepStatement(stmt);
        }
    }

    void TransactionRepository::InsertTransactionPayload(const Payload& payload)
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

        TryBindStatementText(stmt, 1, payload.GetTxHash());
        TryBindStatementText(stmt, 2, payload.GetString1());
        TryBindStatementText(stmt, 3, payload.GetString2());
        TryBindStatementText(stmt, 4, payload.GetString3());
        TryBindStatementText(stmt, 5, payload.GetString4());
        TryBindStatementText(stmt, 6, payload.GetString5());
        TryBindStatementText(stmt, 7, payload.GetString6());
        TryBindStatementText(stmt, 8, payload.GetString7());
        TryBindStatementText(stmt, 9, payload.GetTxHash());

        TryStepStatement(stmt);
    }

    void TransactionRepository::InsertTransactionModel(const CollectData& collectData)
    {
        auto stmt = SetupSqlStatement(R"sql(
            INSERT OR FAIL INTO Transactions (
                Type,
                Id,
                Hash,
                Time,
                String1,
                String2,
                String3,
                String4,
                String5,
                Int1
            ) SELECT ?,?,?,?,?,?,?,?,?,?,?
            WHERE not exists (select 1 from Transactions t where t.Id=?)
        )sql");

        TryBindStatementInt(stmt, 1, (int)*collectData.ptx->GetType());
        TryBindStatementInt64(stmt, 2, collectData.txId);
        TryBindStatementText(stmt, 3, collectData.txHash);
        TryBindStatementInt64(stmt, 4, collectData.ptx->GetTime());
        TryBindStatementText(stmt, 5, collectData.ptx->GetString1());
        TryBindStatementText(stmt, 6, collectData.ptx->GetString2());
        TryBindStatementText(stmt, 7, collectData.ptx->GetString3());
        TryBindStatementText(stmt, 8, collectData.ptx->GetString4());
        TryBindStatementText(stmt, 9, collectData.ptx->GetString5());
        TryBindStatementInt64(stmt, 10, collectData.ptx->GetInt1());
        TryBindStatementInt64(stmt, 2, collectData.txId);

        // TODO (losty-db): insert list!!!

        TryStepStatement(stmt);
    }

    tuple<bool, PTransactionRef> TransactionRepository::CreateTransactionFromListRow(
        const shared_ptr<sqlite3_stmt*>& stmt, bool includedPayload)
    {
        // TODO (aok): move deserialization logic to models ?

        auto[ok0, _txType] = TryGetColumnInt(*stmt, 0);
        auto[ok1, txHash] = TryGetColumnString(*stmt, 1);
        auto[ok2, nTime] = TryGetColumnInt64(*stmt, 2);

        if (!ok0 || !ok1 || !ok2)
            return make_tuple(false, nullptr);

        auto txType = static_cast<TxType>(_txType);
        auto ptx = PocketHelpers::TransactionHelper::CreateInstance(txType);
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

    map<string,int64_t> TransactionRepository::GetTxIds(const vector<string>& txHashes)
    {
        string txReplacers = join(vector<string>(txHashes.size(), "?"), ",");

        auto sql = R"sql(
            select Hash, Id from Transactions where Hash in ( )sql" + txReplacers + R"sql( )
        )sql";

        map<string,int64_t> res;
        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);
            int i = 1;
            for (const auto& hash: txHashes) {
                TryBindStatementText(stmt, i++, hash);
            }

            while (sqlite3_step(*stmt) == SQLITE_ROW) {
                // TODO (losty-db): error
                auto[ok1, hash] = TryGetColumnString(*stmt, 0);
                auto[ok2, txId] = TryGetColumnInt64(*stmt, 1);
                if (ok1 && ok2) res.emplace(hash, txId);
            }

            FinalizeSqlStatement(*stmt);
        });

        return res;
    }

    optional<string> TransactionRepository::TxIdToHash(const int64_t& id)
    {
        auto sql = R"sql(
            select Hash
            from Transactions
            where Id = ?
        )sql";

        optional<string> hash;
        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);
            TryBindStatementInt64(stmt, 1, id);
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto [ok, val] = TryGetColumnString(*stmt, 0); ok)
                    hash = val;
            FinalizeSqlStatement(*stmt);
        });

        return hash;
    }

    optional<int64_t> TransactionRepository::TxHashToId(const string& hash)
    {
        auto sql = R"sql(
            select t.Id
            from Transactions t
            join Registry r
                on r.String = ?
                and r.Id = t.HashId
        )sql";

        optional<int64_t> id;
        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);
            TryBindStatementText(stmt, 1, hash);
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto [ok, val] = TryGetColumnInt64(*stmt, 0); ok)
                    id = val;
            FinalizeSqlStatement(*stmt);
        });

        return id;
    }

    optional<string> TransactionRepository::AddressIdToHash(const int64_t& id)
    {
        auto sql = R"sql(
            select Hash
            from Addresses
            where Id = ?
        )sql";

        optional<string> hash;
        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);
            TryBindStatementInt64(stmt, 1, id);
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto [ok, val] = TryGetColumnString(*stmt, 0); ok)
                    hash = val;
            FinalizeSqlStatement(*stmt);
        });

        return hash;
    }

    optional<int64_t> TransactionRepository::AddressHashToId(const string& hash)
    {
        auto sql = R"sql(
            select Id
            from Addresses
            where Hash = ?
        )sql";

        optional<int64_t> id;
        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);
            TryBindStatementText(stmt, 1, hash);
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto [ok, val] = TryGetColumnInt64(*stmt, 0); ok)
                    id = val;
            FinalizeSqlStatement(*stmt);
        });

        return id;
    }

} // namespace PocketDb

