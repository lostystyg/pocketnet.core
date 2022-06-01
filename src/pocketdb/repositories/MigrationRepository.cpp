// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/repositories/MigrationRepository.h"

namespace PocketDb
{
    bool MigrationRepository::SplitLikers()
    {
        bool result = false;

        TryTransactionStep(__func__, [&]()
        {
            if (CheckNeedSplitLikers())
            {
                result = true;
                return;
            }

            LogPrint(BCLog::MIGRATION, "SQLDB Migration: SplitLikers starting. Do not turn off your node and PC.\n");

            auto stmt = SetupSqlStatement(R"sql(
                insert into Ratings (type, last, height, id, value)
                select

                (
                    select
                    case sc.Type
                        when 300 then 11
                        when 301 then (
                        case c.String5
                            when null then 12
                            else 10
                        end
                        )
                    end
                    from Transactions sc indexed by Transactions_Height_Id
                    join Transactions ul indexed by Transactions_Type_Last_String1_Height_Id on ul.Type = 100 and ul.Last = 1 and ul.Height > 0 and ul.String1 = sc.String1
                    join Transactions c on c.Hash = sc.String2
                    join Transactions ua indexed by Transactions_Type_Last_String1_Height_Id on ua.Type = 100 and ua.Last = 1 and ua.Height > 0 and ua.String1 = c.String1
                    where sc.Type in (300,301)
                    and sc.Height = r.Height
                    and ua.Id = r.Id
                    and ul.Id = r.Value
                    order by sc.BlockNum asc
                    limit 1
                ) lkrType

                , 1
                , r.Height
                , r.Id
                , r.Value

                from Ratings r
                where r.Type in (1)
            )sql");

            TryStepStatement(stmt);

            result = CheckNeedSplitLikers();
        });

        return result;
    }

    bool MigrationRepository::CheckNeedSplitLikers()
    {
        bool result = false;

        auto stmt = SetupSqlStatement(R"sql(
            select 'All', ifnull(sum(rAll.Id),0)sAllId, ifnull(sum(rAll.Value),0)sAllValue, count()cnt
            from Ratings rAll
            where rAll.Type in (1)

            union

            select 'Split', ifnull(sum(r.Id),0)sId, ifnull(sum(r.Value),0)sValue, count()cnt
            from Ratings r
            where r.Type in (10,11,12)
        )sql");

        if (sqlite3_step(*stmt) == SQLITE_ROW)
        {
            auto[sumAllIdOk, sumAllId] = TryGetColumnInt64(*stmt, 1);
            auto[sumAllValueOk, sumAllValue] = TryGetColumnInt64(*stmt, 2);
            auto[cntAllOk, cntAll] = TryGetColumnInt64(*stmt, 3);

            if (sqlite3_step(*stmt) == SQLITE_ROW)
            {
                auto[sumSpltIdOk, sumSpltId] = TryGetColumnInt64(*stmt, 1);
                auto[sumSpltValueOk, sumSpltValue] = TryGetColumnInt64(*stmt, 2);
                auto[cntSpltOk, cntSplt] = TryGetColumnInt64(*stmt, 3);

                result = (sumAllId == sumSpltId && sumAllValue == sumSpltValue && cntAll == cntSplt);
            }
        }

        FinalizeSqlStatement(*stmt);

        return result;
    }


} // namespace PocketDb

