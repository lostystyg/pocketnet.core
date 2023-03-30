// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/repositories/web/ModerationRepository.h"

namespace PocketDb
{
    void ModerationRepository::Init() {}

    void ModerationRepository::Destroy() {}

    UniValue ModerationRepository::GetJury(const string& jury)
    {
        UniValue result(UniValue::VOBJ);

        SqlTransaction(__func__, [&]()
        {
            Sql(R"sql(
                with
                    flag as (
                        select
                            ROWID
                        from
                            Transactions
                        where
                            Hash = ?
                    ),
                    juryRec as (
                        select
                            j.AccountId,
                            j.Reason
                        from
                            Jury j,
                            flag
                        where
                            j.FlagRowId = flag.ROWID
                    ),
                    account as (
                        select
                            u.String1 as AddressHash
                        from
                            Transactions u indexed by Transactions_Id_First,
                            juryRec
                        where
                            u.Id = juryRec.AccountId and
                            u.First = 1
                    ),
                    juryVerd as (
                        select
                            jv.Verdict
                        from
                            juryVerdict jv,
                            flag
                        where
                            jv.FlagRowId = flag.ROWID
                    )
                select
                    a.AddressHash,
                    j.Reason,
                    ifnull(jv.Verdict, -1)
                from
                    juryRec j
                    join account a
                    left join juryVerd jv
            )sql")
            .Bind(jury)
            .Select([&](Cursor& cursor) {
                if (cursor.Step())
                {
                    result.pushKV("id", jury);
                    cursor.Collect<std::string>(0, result, "address");
                    cursor.Collect<int>(1, result, "address");
                    if (auto [ok, value] = cursor.TryGetColumnInt(2); ok)
                    {
                        result.pushKV("verdict", value);

                        // TODO (aok): add object with ban information if verditc == 1
                    }
                }
            });
        });

        return result;
    }

    UniValue ModerationRepository::GetAllJury()
    {
        UniValue result(UniValue::VARR);

        SqlTransaction(__func__, [&]()
        {
            Sql(R"sql(
                select
                    f.Hash,
                    f.String3,
                    j.Reason,
                    ifnull(jv.Verdict, -1)
                from
                    Jury j
                    cross join Transactions f
                        on f.ROWID = j.FlagRowId
                    left join JuryVerdict jv
                        on jv.FlagRowId = j.FlagRowId
            )sql")
            .Select([&](Cursor& cursor) {
                while (cursor.Step())
                {
                    UniValue rcrd(UniValue::VOBJ);

                    cursor.Collect<std::string>(0, rcrd, "id");
                    cursor.Collect<std::string>(1, rcrd, "address");
                    cursor.Collect<int>(2, rcrd, "reason");
                    cursor.Collect<int>(3, rcrd, "verdict");

                    result.push_back(rcrd);
                }
            });
        });

        return result;
    }

    vector<JuryContent> ModerationRepository::GetJuryAssigned(const string& address, bool verdict, const Pagination& pagination)
    {
        vector<JuryContent> result;

        SqlTransaction(__func__, [&]()
        {
            string orderBy = " order by f.Height ";
            orderBy += (pagination.Desc ? " desc " : " asc ");

            Sql(R"sql(
                select

                    f.Hash as FlagHash,
                    f.Height as FlagHeight,
                    f.Int1 as Reason,
                    c.Id as ContentId,
                    c.Type as ContentType

                from Transactions u indexed by Transactions_Type_Last_String1_Height_Id
                cross join JuryModerators jm indexed by JuryModerators_AccountId_FlagRowId
                    on jm.AccountId = u.Id
                cross join Transactions f
                    on f.ROWID = jm.FlagRowId
                cross join Transactions c
                    on c.Hash = f.String2

                where u.Type in (100)
                  and u.Last = 1
                  and u.Height is not null
                  and u.String1 = ? 
                  and f.Height <= ?

                  and )sql" + string(verdict ? "" : "not") + R"sql( exists (
                     select 1
                     from Transactions v indexed by Transactions_Type_String1_String2_Height
                     where
                        v.Type = 420 and
                        v.String1 = u.String1 and
                        v.String2 = f.Hash and
                        v.Height > 0
                  )

                )sql" + orderBy + R"sql(

                limit ? offset ?
            )sql")
            .Bind(
                address,
                pagination.TopHeight,
                pagination.PageSize,
                pagination.PageStart
            )
            .Select([&](Cursor& cursor) {
                UniValue record(UniValue::VOBJ);

                cursor.Collect<std::string>(0, record, "juryid"); // TODO (aok): is it string?
                cursor.Collect<int64_t>(1, record, "height");
                cursor.Collect<int>(2, record, "reason");

                int64_t contentId; int contentType;
                if (cursor.CollectAll(contentId, contentType)) {
                    result.push_back(JuryContent{contentId, (TxType)contentType, record});
                }
            });
        });

        return result;
    }

    UniValue ModerationRepository::GetJuryModerators(const string& jury)
    {
        UniValue result(UniValue::VARR);

        SqlTransaction(__func__, [&]()
        {
            Sql(R"sql(
                with
                    flag as (
                        select
                            ROWID
                        from
                            Transactions
                        where
                            Hash = ?
                    ),
                    jurymod as (
                        select
                            jm.AccountId
                        from
                            JuryModerators jm,
                            flag
                        where
                            jm.FlagRowId = flag.ROWID
                    ),
                    moderators as (
                        select
                            u.String1 as Address
                        from
                            Transactions u indexed by Transactions_Id_First,
                            jurymod
                        where
                            u.Id = jurymod.AccountId and
                            u.First = 1
                    )

                select
                    m.Address
                from
                    moderators m
            )sql")
            .Bind(jury)
            .Select([&](Cursor& cursor) {
                while (cursor.Step())
                {
                    if (auto[ok, value] = cursor.TryGetColumnString(0); ok)
                        result.push_back(value);
                }
            });
        });

        return result;
    }

    UniValue ModerationRepository::GetBans(const string& address)
    {
        UniValue result(UniValue::VARR);

        SqlTransaction(__func__, [&]()
        {
            Sql(R"sql(
                select
                    f.Hash as JuryId,
                    f.Int1 as Reason,
                    b.Ending
                from
                    Transactions u indexed by Transactions_Type_Last_String1_Height_Id
                    cross join JuryBan b indexed by JuryBan_AccountId_Ending
                        on b.AccountId = u.Id
                    cross join Transactions v
                        on v.ROWID = b.VoteRowId
                    cross join Transactions f
                        on f.Hash = v.String2
                where
                    u.Type = 100 and
                    u.Last = 1 and
                    u.String1 = ? and
                    u.Height > 0
            )sql")
            .Bind(address)
            .Select([&](Cursor& cursor) {
                while (cursor.Step())
                {
                    UniValue record(UniValue::VOBJ);
                    
                    if (auto[ok, value] = cursor.TryGetColumnString(0); ok)
                        record.pushKV("juryId", value);
                    if (auto[ok, value] = cursor.TryGetColumnInt(1); ok)
                        record.pushKV("reason", value);
                    if (auto[ok, value] = cursor.TryGetColumnInt64(2); ok)
                        record.pushKV("ending", value);
                    
                    result.push_back(record);
                }
            });
        });

        return result;
    }

}