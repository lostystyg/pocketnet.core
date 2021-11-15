// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/repositories/web/SearchRepository.h"

namespace PocketDb
{
    void SearchRepository::Init() {}

    void SearchRepository::Destroy() {}

    UniValue SearchRepository::SearchTags(const SearchRequest& request)
    {
        UniValue result(UniValue::VARR);

        string keyword = "%" + request.Keyword + "%";
        string sql = R"sql(
            select Value
            from Tags t indexed by Tags_Value
            where t.Value match ?
            limit ?
            offset ?
        )sql";

        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);
            TryBindStatementText(stmt, 1, keyword);
            TryBindStatementInt(stmt, 2, request.PageSize);
            TryBindStatementInt(stmt, 3, request.PageStart);

            while (sqlite3_step(*stmt) == SQLITE_ROW)
            {
                if (auto[ok, value] = TryGetColumnString(*stmt, 0); ok)
                    result.push_back(value);
            }

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    vector<int64_t> SearchRepository::SearchIds(const SearchRequest& request)
    {
        auto func = __func__;
        vector<int64_t> ids;

        if (request.Keyword.empty())
            return ids;

        // First search request
        string fieldTypes = join(request.FieldTypes | transformed(static_cast<std::string(*)(int)>(std::to_string)), ",");
        string txTypes = join(request.TxTypes | transformed(static_cast<std::string(*)(int)>(std::to_string)), ",");
        string heightWhere = request.TopBlock > 0 ? " and t.Height <= ? " : "";
        string addressWhere = !request.Address.empty() ? " and t.String1 = ? " : "";

        string sql = R"sql(
            select t.Id
            from Transactions t indexed by Transactions_Type_Last_String1_Height_Id
            where t.Type in ( )sql" + txTypes + R"sql( )
                and t.Last = 1
                and t.Height is not null
                )sql" + heightWhere + R"sql(
                )sql" + addressWhere + R"sql(
                and t.Id in (
                    select cm.ContentId
                    from web.Content c, web.ContentMap cm
                    where c.ROWID = cm.ROWID
                        and cm.FieldType in ( )sql" + fieldTypes + R"sql( )
                        and c.Value match ?
                    )
            order by t.Id desc
            limit ?
            offset ?
        )sql";
        
        TryTransactionStep(func, [&]()
        {
            auto stmt = SetupSqlStatement(sql);

            int i = 1;
            if (request.TopBlock > 0)
                TryBindStatementInt(stmt, i++, request.TopBlock);
            if (!request.Address.empty())
                TryBindStatementText(stmt, i++, request.Address);
            TryBindStatementText(stmt, i++, request.Keyword + "*");
            TryBindStatementInt(stmt, i++, request.PageSize);
            TryBindStatementInt(stmt, i++, request.PageStart);

            LogPrint(BCLog::SQL, "%s: %s\n", func, sqlite3_expanded_sql(*stmt));

            while (sqlite3_step(*stmt) == SQLITE_ROW)
            {
                if (auto[ok, value] = TryGetColumnInt64(*stmt, 0); ok)
                    ids.push_back(value);
            }

            FinalizeSqlStatement(*stmt);
        });

        return ids;
    }

    map<int, string> SearchRepository::SearchUsers(const string& searchstr, const vector<int> fieldTypes, bool orderbyrank)
    {
        auto func = __func__;
        map<int, string> result;

        string sql = R"sql(
            select
                t.Id,
                f.Value,
                fm.FieldType
            from web.Content f
            join web.ContentMap fm on fm.ROWID = f.ROWID
            join Transactions t on t.Id = fm.ContentId
            join Payload p on p.TxHash=t.Hash
            where t.Last = 1
                and t.Type = 100
                and t.Height is not null
                and fm.FieldType in ( )sql" + join(vector<string>(fieldTypes.size(), "?"), ",") + R"sql( )
                and f.Value match ?
        )sql";

        if (orderbyrank)
            sql += " order by rank ";


        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);

            int i = 1;
            for (const auto& fieldtype: fieldTypes)
                TryBindStatementInt(stmt, i++, fieldtype);
                
            TryBindStatementText(stmt, i++, "\"" + searchstr + "\"" + " OR " + searchstr + "*");

            LogPrint(BCLog::SQL, "%s: %s\n", func, sqlite3_expanded_sql(*stmt));

            while (sqlite3_step(*stmt) == SQLITE_ROW)
            {
                auto[ok0, id] = TryGetColumnInt(*stmt, 0);
                auto[ok, value] = TryGetColumnString(*stmt, 1);
                result[id] = value;
            }

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

}