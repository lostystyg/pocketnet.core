// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include <boost/format.hpp>
#include "pocketdb/repositories/web/WebRepository.h"

namespace PocketDb
{
    int WebRepository::GetCurrentHeight()
    {
        int result = 1;

        SqlTransaction(__func__, [&]()
        {
            // Delete
            Sql(R"sql(
                select
                    Value
                from
                    web.System
                where
                    Key = 'LastBlock'
                limit 1
            )sql")
            .Select([&](Cursor& cursor) {
                if (cursor.Step())
                    cursor.CollectAll(result);
            });
        });

        return result;
    }

    void WebRepository::SetCurrentHeight(int height)
    {
        SqlTransaction(__func__, [&]()
        {
            // Delete
            Sql(R"sql(
                insert into web.System (Key, Value) values ('LastBlock', ?)
                on conflict (Key) do update set Value = ? where Key = 'LastBlock'
            )sql")
            .Bind(height, height)
            .Run();
        });
    }

    vector<WebTag> WebRepository::GetContentTags(int height)
    {
        vector<WebTag> result;

        string sql = R"sql(
            select distinct
                c.Uid,
                pp.String1,
                json_each.value

            from Transactions p

            join Payload pp on
                pp.TxId = p.RowId

            join json_each(pp.String4)

            join Chain c on
                c.TxId = p.RowId and
                c.Height = ?

            join Last l on
                l.TxId = p.RowId

            where
                p.Type in (200, 201, 202, 209, 210)
        )sql";

        SqlTransaction(
            __func__,
            [&]() -> Stmt& {
                return Sql(sql).Bind(height);
            },
            [&] (Stmt& stmt) {
                stmt.Select([&](Cursor& cursor) {
                    while (cursor.Step())
                    {
                        auto[okId, id] = cursor.TryGetColumnInt64(0);
                        if (!okId) continue;

                        auto[okLang, lang] = cursor.TryGetColumnString(1);
                        if (!okLang) continue;

                        auto[okValue, value] = cursor.TryGetColumnString(2);
                        if (!okValue) continue;

                        result.emplace_back(WebTag(id, lang, value));
                    }
                });
            }
        );

        return result;
    }

    void WebRepository::UpsertContentTags(const vector<WebTag>& contentTags)
    {
        // build distinct lists
        vector<int> ids;
        for (auto& contentTag : contentTags)
        {
            if (find(ids.begin(), ids.end(), contentTag.ContentId) == ids.end())
                ids.emplace_back(contentTag.ContentId);
        }
        
        map<string, vector<string>> tags;
        for (auto& contentTag : contentTags)
        {
            
            if (find(tags[contentTag.Lang].begin(), tags[contentTag.Lang].end(), contentTag.Value) == tags[contentTag.Lang].end())
                tags[contentTag.Lang].push_back(contentTag.Value);
        }

        // Next work in transaction
        SqlTransaction(__func__, [&]()
        {
            // Insert new tags and ignore exists with unique index Lang+Value
            for (const auto& lang: tags)
            {
                for (const auto& tag : lang.second)
                {
                    Sql(R"sql(
                        insert or ignore
                        into web.Tags (Lang, Value, Count)
                        values (?, ?, 0)
                    )sql")
                    .Bind(lang.first, tag)
                    .Run();
                }
            }

            // Delete exists mappings ContentId <-> TagId
            Sql(R"sql(
                delete from web.TagsMap
                where ContentId in ( )sql" + join(vector<string>(ids.size(), "?"), ",") + R"sql( )
            )sql")
            .Bind(ids)
            .Run();

            // Insert new mappings ContentId <-> TagId
            for (const auto& contentTag : contentTags)
            {
                Sql(R"sql(
                    insert or ignore
                    into web.TagsMap (ContentId, TagId) values (
                        ?,
                        (select t.Id from web.Tags t where t.Lang = ? and t.Value = ?)
                    )
                )sql")
                .Bind(contentTag.ContentId, contentTag.Lang, contentTag.Value)
                .Run();
            }

            // Update count of contents in updated tags
            for (const auto& lang: tags)
            {
                for (const auto& tag : lang.second)
                {
                    Sql(R"sql(
                        update Tags
                        set Count = ifnull((select count() from TagsMap tm where tm.TagId = Tags.Id), 0)
                        where Tags.Lang = ? and Tags.Value = ?
                    )sql")
                    .Bind(lang.first, tag)
                    .Run();
                }
            }
        });
    }

    vector<WebContent> WebRepository::GetContent(int height)
    {
        vector<WebContent> result;

        string sql = R"sql(
            select
                t.Type,
                c.Uid,
                p.String1,
                p.String2,
                p.String3,
                p.String4,
                p.String5,
                p.String6,
                p.String7

            from Transactions t

            join Chain c on
                c.TxId = t.RowId and
                c.Height = ?

            join Payload p on
                p.TxId = t.RowId

            where
                t.Type in (100, 200, 201, 202, 209, 210, 204, 205)
        )sql";
       
        SqlTransaction(
            __func__,
            [&]() -> Stmt& {
                return Sql(sql).Bind(height);
            },
            [&] (Stmt& stmt) {
                stmt.Select([&](Cursor& cursor) {
                    while (cursor.Step())
                    {
                        auto[okType, type] = cursor.TryGetColumnInt(0);
                        auto[okId, id] = cursor.TryGetColumnInt64(1);
                        if (!okType || !okId)
                            continue;

                        switch ((TxType)type)
                        {
                        case ACCOUNT_USER:

                            if (auto[ok, string2] = cursor.TryGetColumnString(3); ok)
                                result.emplace_back(WebContent(id, ContentFieldType_AccountUserName, string2));

                            if (auto[ok, string4] = cursor.TryGetColumnString(5); ok)    
                                result.emplace_back(WebContent(id, ContentFieldType_AccountUserAbout, string4));

                            // if (auto[ok, string5] = cursor.TryGetColumnString(6); ok)
                            //     result.emplace_back(WebContent(id, ContentFieldType_AccountUserUrl, string5));

                            break;
                        case CONTENT_POST:

                            if (auto[ok, string2] = cursor.TryGetColumnString(3); ok)
                                result.emplace_back(WebContent(id, ContentFieldType_ContentPostCaption, string2));
                            
                            if (auto[ok, string3] = cursor.TryGetColumnString(4); ok)
                                result.emplace_back(WebContent(id, ContentFieldType_ContentPostMessage, string3));

                            // if (auto[ok, string7] = cursor.TryGetColumnString(8); ok)
                            //     result.emplace_back(WebContent(id, ContentFieldType_ContentPostUrl, string7));

                            break;
                        case CONTENT_VIDEO:

                            if (auto[ok, string2] = cursor.TryGetColumnString(3); ok)
                                result.emplace_back(WebContent(id, ContentFieldType_ContentVideoCaption, string2));

                            if (auto[ok, string3] = cursor.TryGetColumnString(4); ok)
                                result.emplace_back(WebContent(id, ContentFieldType_ContentVideoMessage, string3));

                            // if (auto[ok, string7] = cursor.TryGetColumnString(8); ok)
                            //     result.emplace_back(WebContent(id, ContentFieldType_ContentVideoUrl, string7));

                            break;
                        
                        // TODO (aok): parse JSON for indexing
                        // case CONTENT_ARTICLE:

                        // case CONTENT_COMMENT:
                        // case CONTENT_COMMENT_EDIT:

                            // TODO (aok): implement extract message from JSON
                            // if (auto[ok, string1] = cursor.TryGetColumnString(2); ok)
                            //     result.emplace_back(WebContent(id, ContentFieldType_CommentMessage, string1));

                            // break;
                        default:
                            break;
                        }
                    }
                });
            }
        );

        return result;
    }

    void WebRepository::UpsertContent(const vector<WebContent>& contentList)
    {
        vector<int64_t> ids;
        for (auto& contentItm : contentList)
        {
            if (find(ids.begin(), ids.end(), contentItm.ContentId) == ids.end())
                ids.emplace_back(contentItm.ContentId);
        }

        // ---------------------------------------------------------

        SqlTransaction(__func__, [&]()
        {
            // ---------------------------------------------------------
            int64_t nTime1 = GetTimeMicros();

            Sql(R"sql(
                delete from web.Content
                where ROWID in (
                    select cm.ROWID from ContentMap cm where cm.ContentId in (
                        )sql" + join(vector<string>(ids.size(), "?"), ",") + R"sql(
                    )
                )
            )sql")
            .Bind(ids)
            .Run();

            // ---------------------------------------------------------
            int64_t nTime2 = GetTimeMicros();

            Sql(R"sql(
                delete from web.ContentMap
                where ContentId in (
                    )sql" + join(vector<string>(ids.size(), "?"), ",") + R"sql(
                )
            )sql")
            .Bind(ids)
            .Run();

            // ---------------------------------------------------------
            int64_t nTime3 = GetTimeMicros();

            for (const auto& contentItm : contentList)
            {
                SetLastInsertRowId(0);

                Sql(R"sql(
                    insert or ignore into ContentMap (ContentId, FieldType) values (?,?)
                )sql")
                .Bind(contentItm.ContentId, (int)contentItm.FieldType)
                .Run();

                // ---------------------------------------------------------

                auto lastRowId = GetLastInsertRowId();
                if (lastRowId > 0)
                {
                    Sql(R"sql(
                        replace into web.Content (ROWID, Value) values (?,?)
                    )sql")
                    .Bind(lastRowId, contentItm.Value)
                    .Run();
                }
                else
                {
                    LogPrintf("Warning: content (%d) field (%d) not indexed in search db\n",
                        contentItm.ContentId, (int)contentItm.FieldType);
                }
            }

            // ---------------------------------------------------------
            int64_t nTime4 = GetTimeMicros();

            LogPrint(
                BCLog::BENCH,
                "        - SqlTransaction (%s): %.2fms + %.2fms + %.2fms = %.2fms\n",
                __func__,
                0.001 * (nTime2 - nTime1),
                0.001 * (nTime3 - nTime2),
                0.001 * (nTime4 - nTime3),
                0.001 * (nTime4 - nTime1)
            );
        });
    }

    void WebRepository::UpsertBarteronAccounts(int height)
    {
        SqlTransaction(__func__, [&]()
        {
            // Delete
            Sql(R"sql(
                delete from web.BarteronAccountTags
                where
                    web.BarteronAccountTags.AccountId in (
                        select
                            c.Uid
                        from
                            Transactions t
                        cross join
                            Chain c indexed by Chain_TxId_Height
                                on c.TxId = t.RowId and c.Height = ?
                        where
                            t.Type in (104)
                    )
            )sql")
            .Bind(height)
            .Run();

            // Add
            Sql(R"sql(
                insert into web.BarteronAccountTags (AccountId, Tag)
                select distinct
                    c.Uid,
                    pj.value
                from
                    Transactions t
                cross join
                    Chain c indexed by Chain_TxId_Height
                        on c.TxId = t.RowId and c.Height = ?
                cross join
                    Payload p
                        on p.TxId = t.RowId
                cross join
                    json_each(p.String4, '$.a') as pj
                where
                    t.Type = 104 and
                    json_valid(p.String4) and
                    json_type(p.String4, '$.a') = 'array'
            )sql")
            .Bind(height)
            .Run();
        });
    }

    void WebRepository::UpsertBarteronOffers(int height)
    {
        SqlTransaction(__func__, [&]()
        {
            // Delete
            Sql(R"sql(
                delete from web.BarteronOffers
                where
                    web.BarteronOffers.ROWID in (
                        select
                            bo.ROWID
                        from
                            Chain c indexed by Chain_Height_Uid
                        cross join
                            BarteronOffers bo indexed by BarteronOffers_OfferId_Tag_AccountId
                                on bo.OfferId = c.Uid
                        cross join
                            Transactions t
                                on t.RowId = c.TxId and t.Type = 211
                        where
                            c.Height = ?

                    )
            )sql")
            .Bind(height)
            .Run();

            // Add offer
            Sql(R"sql(
                insert into web.BarteronOffers (AccountId, OfferId, Tag)
                select
                    cu.Uid as AccountId,
                    ct.Uid as OfferId,
                    json_extract(p.String4, '$.t') as Tag
                from
                    Transactions t
                cross join
                    Chain ct indexed by Chain_TxId_Height
                        on ct.TxId = t.RowId and ct.Height = ?
                cross join
                    Transactions u indexed by Transactions_Type_RegId1_RegId2_RegId3
                        on u.Type = 104 and u.RegId1 = t.RegId1
                cross join
                    Last lu
                        on lu.TxId = u.RowId
                cross join
                    Chain cu on
                        cu.TxId = u.RowId
                cross join
                    Payload p -- primary key
                        on p.TxId = t.RowId
                where
                    t.Type = 211 and
                    json_valid(p.String4)
            )sql")
            .Bind(height)
            .Run();

            // Remove allowed tags
            Sql(R"sql(
                delete from web.BarteronOfferTags
                where
                    web.BarteronOfferTags.ROWID in (
                        select
                            bot.ROWID
                        from
                            Chain c indexed by Chain_Height_Uid
                        cross join
                            BarteronOfferTags bot
                                on bot.OfferId = c.Uid
                        cross join
                            Transactions t
                                on t.RowId = c.TxId and t.Type = 211
                        where
                            c.Height = ?

                    )
            )sql")
            .Bind(height)
            .Run();
            
            // Add allowed tags
            Sql(R"sql(
                insert into web.BarteronOfferTags (OfferId, Tag)
                select distinct
                    ct.Uid as OfferId,
                    pj.value as Tag
                from
                    Transactions t
                cross join
                    Chain ct indexed by Chain_TxId_Height
                        on ct.TxId = t.RowId and ct.Height = ?
                cross join
                    Payload p -- primary key
                        on p.TxId = t.RowId
                cross join
                    json_each(p.String4, '$.a') as pj
                where
                    t.Type = 211 and
                    json_valid(p.String4) and
                    json_type(p.String4, '$.a') = 'array'
            )sql")
            .Bind(height)
            .Run();
        });
    }

    void WebRepository::CollectAccountStatistic()
    {
        int64_t nTime2 = GetTimeMicros();

        // PostsCount
        SqlTransaction(__func__, [&]()
        {
            Sql(R"sql(
                delete from web.AccountStatistic where Type = 1
            )sql").Run();

            Sql(R"sql(
                insert into web.AccountStatistic (AccountRegId, Type, Data)
                select
                    t.RegId1,
                    1,
                    count()
                from
                    Transactions t
                cross join
                    Last l on
                        l.TxId = t.RowId
                cross join
                    Transactions po indexed by Transactions_Type_RegId1_RegId2_RegId3 on
                        po.Type in (200,201,202,209,210) and
                        po.RegId1 = t.RegId1
                cross join
                    Last lpo
                        on lpo.TxId = po.RowId
                where
                    t.Type = 100
                group by
                    t.RegId1
            )sql").Run();
        });

        int64_t nTime3 = GetTimeMicros();
        LogPrintf("CollectAccountStatistic: PostsCount %.2fms\n", 0.001 * (double)(nTime3 - nTime2));

        // DelCount
        SqlTransaction(__func__, [&]()
        {
            Sql(R"sql(
                delete from web.AccountStatistic where Type = 2
            )sql").Run();

            Sql(R"sql(
                insert into web.AccountStatistic (AccountRegId, Type, Data)
                select
                    t.RegId1,
                    2,
                    count()
                from
                    Transactions t
                cross join
                    Last l on
                        l.TxId = t.RowId
                cross join
                    Transactions po indexed by Transactions_Type_RegId1_RegId2_RegId3 on
                        po.Type in (207) and
                        po.RegId1 = t.RegId1
                cross join
                    Last lpo
                        on lpo.TxId = po.RowId
                where
                    t.Type = 100
                group by
                    t.RegId1
            )sql").Run();
        });

        int64_t nTime4 = GetTimeMicros();
        LogPrintf("CollectAccountStatistic: DelCount %.2fms\n", 0.001 * (double)(nTime4 - nTime3));

        // SubscribesCount
        SqlTransaction(__func__, [&]()
        {
            Sql(R"sql(
                delete from web.AccountStatistic where Type = 3
            )sql").Run();

            Sql(R"sql(
                insert into web.AccountStatistic (AccountRegId, Type, Data)
                select
                    t.RegId1,
                    3,
                    count()
                from
                    Transactions t
                cross join
                    Last l on
                        l.TxId = t.RowId
                cross join
                    Transactions subs indexed by Transactions_Type_RegId1_RegId2_RegId3 on
                        subs.Type in (302, 303) and
                        subs.RegId1 = t.RegId1
                cross join
                    Last lsubs
                        on lsubs.TxId = subs.RowId
                cross join
                    Transactions uas indexed by Transactions_Type_RegId1_RegId2_RegId3
                        on uas.Type in (100) and uas.RegId1 = subs.RegId2
                cross join
                    Last luas
                        on luas.TxId = uas.RowId
                where
                    t.Type = 100
                group by
                    t.RegId1
            )sql").Run();
        });

        int64_t nTime5 = GetTimeMicros();
        LogPrintf("CollectAccountStatistic: SubscribesCount %.2fms\n", 0.001 * (double)(nTime5 - nTime4));
            
        // SubscribersCount
        SqlTransaction(__func__, [&]()
        {
            Sql(R"sql(
                delete from web.AccountStatistic where Type = 4
            )sql").Run();

            Sql(R"sql(
                insert into web.AccountStatistic (AccountRegId, Type, Data)
                select
                    t.RegId1,
                    4,
                    count()
                from
                    Transactions t
                cross join
                    Last l on
                        l.TxId = t.RowId
                cross join
                    Transactions subs indexed by Transactions_Type_RegId2_RegId1 on
                        subs.Type in (302, 303) and
                        subs.RegId2 = t.RegId1
                cross join
                    Last lsubs
                        on lsubs.TxId = subs.RowId
                cross join
                    Transactions uas indexed by Transactions_Type_RegId1_RegId2_RegId3
                        on uas.Type in (100) and uas.RegId1 = subs.RegId1
                cross join
                    Last luas
                        on luas.TxId = uas.RowId
                where
                    t.Type = 100
                group by
                    t.RegId1
            )sql").Run();
        });

        int64_t nTime6 = GetTimeMicros();
        LogPrintf("CollectAccountStatistic: SubscribersCount %.2fms\n", 0.001 * (double)(nTime6 - nTime5));

        // FlagsJson
        SqlTransaction(__func__, [&]()
        {
            Sql(R"sql(
                delete from web.AccountStatistic where Type = 5
            )sql").Run();

            Sql(R"sql(
                insert into web.AccountStatistic (AccountRegId, Type, Data)
                select
                    gr.AccId,
                    5,
                    json_group_object(gr.Type, gr.Cnt)
                from (
                    select
                        t.RegId1 as AccId,
                        f.Int1 as Type,
                        count() as Cnt
                    from
                        Transactions t
                    cross join
                        Last l on
                            l.TxId = t.RowId
                    cross join
                        Transactions f indexed by Transactions_Type_RegId3_RegId1 on
                            f.Type in (410) and
                            f.RegId3 = t.RegId1
                    cross join
                        Chain c on
                            c.TxId = f.RowId
                    where
                        t.Type = 100
                    group by
                        t.RegId1, f.Int1
                )gr
                group by
                    gr.AccId
            )sql").Run();
        });

        int64_t nTime7 = GetTimeMicros();
        LogPrintf("CollectAccountStatistic: FlagsJson %.2fms\n", 0.001 * (double)(nTime7 - nTime6));

        // TODO - need optimization or remove this parameter
        // FirstFlagsCount
        SqlTransaction(__func__, [&]()
        {
            Sql(R"sql(
                delete from web.AccountStatistic where Type = 6
            )sql").Run();

            Sql(R"sql(
                insert into web.AccountStatistic (AccountRegId, Type, Data)
                select
                    gr.AccRegId,
                    6,
                    json_group_object(gr.Type, gr.Cnt)
                from (
                    select
                        gr.AccRegId,
                        gr.Type,
                        count() as Cnt
                    from (
                        select
                            f.RegId3 as AccRegId,
                            f.Int1 as Type,
                            cf.Height,
                            min(cfp.Height) as minHeight
                        from
                            Transactions f indexed by Transactions_Type_RegId3_RegId1
                        cross join
                            Transactions fp indexed by Transactions_Type_RegId1_RegId2_RegId3 on
                                fp.Type in (200, 201, 202, 209, 210) and
                                fp.RegId1 = f.RegId3
                        cross join
                            First ffp
                                on ffp.TxId = fp.RowId
                        cross join
                            Chain cfp indexed by Chain_TxId_Height
                                on cfp.TxId = fp.RowId
                        cross join
                            Chain cf indexed by Chain_TxId_Height
                                on cf.TxId = f.RowId
                        where
                            f.Type in (410)
                        group by
                            f.RegId3, f.Int1
                    )gr
                    where
                        gr.Height >= gr.minHeight and
                        gr.Height <= (gr.minHeight + (14 * 1440))
                    group by
                        gr.AccRegId,
                        gr.Type
                )gr
                group by
                    gr.AccRegId
            )sql").Run();
        });

        int64_t nTime8 = GetTimeMicros();
        LogPrintf("CollectAccountStatistic: FirstFlagsCount %.2fms\n", 0.001 * (double)(nTime8 - nTime7));

        // ActionsCount
        SqlTransaction(__func__, [&]()
        {
            Sql(R"sql(
                delete from web.AccountStatistic where Type = 7
            )sql").Run();

            Sql(R"sql(
                insert into web.AccountStatistic (AccountRegId, Type, Data)
                select
                    t.RegId1,
                    7,
                    count()
                from
                    Transactions t
                where
                    t.Type >= 100
                group by
                    t.RegId1
            )sql").Run();
        });

        int64_t nTime9 = GetTimeMicros();
        LogPrintf("CollectAccountStatistic: ActionsCount %.2fms\n", 0.001 * (double)(nTime9 - nTime8));

        // Last 5 Contents
        SqlTransaction(__func__, [&]()
        {
            Sql(R"sql(
                delete from web.AccountStatistic where Type = 8
            )sql").Run();

            Sql(R"sql(
                insert into web.AccountStatistic (AccountRegId, Type, Data)
                select
                    t.RegId1,
                    8,
                    ifnull((
                        select sum(ifnull(ptr.Value,0))
                        from (
                            select cpt.Uid
                            from Transactions pt indexed by Transactions_Type_RegId1_RegId2_RegId3
                            join Chain cpt on cpt.TxId = pt.RowId
                            join Last lpt on lpt.TxId = pt.RowId
                            where pt.Type in ( 200,201,202,209,210,211 )
                                and pt.RegId1 = t.RegId1
                                and cpt.Height < ctml.Height
                                and cpt.Height > (ctml.Height - 43200)
                            order by cpt.Height desc
                            limit 5
                        )q
                        left join Ratings ptr indexed by Ratings_Type_Uid_Last_Height
                            on ptr.Type = 2 and ptr.Uid = q.Uid and ptr.Last = 1
                    ), 0)SumRating
                from
                    Transactions t
                cross join
                    Last l on
                        l.TxId = t.RowId
                cross join
                    Transactions tm on
                        tm.Type in ( 200,201,202,209,210,211 ) and
                        tm.RegId1 = t.RegId1 and
                        tm.RowId = (select max(tml.RowId) from Transactions tml where tml.Type in ( 200,201,202,209,210,211 ) and tml.RegId1 = t.RegId1)
                cross join
                    Chain ctml on
                        ctml.TxId = tm.RowId
                where
                    t.Type in (100)
            )sql").Run();
        });

        int64_t nTime10 = GetTimeMicros();
        LogPrintf("CollectAccountStatistic: Last 5 Contents %.2fms\n", 0.001 * (double)(nTime10 - nTime9));
    }

}