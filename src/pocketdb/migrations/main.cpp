// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/migrations/main.h"

namespace PocketDb
{
    PocketDbMainMigration::PocketDbMainMigration() : PocketDbMigration()
    {
        _tables.emplace_back(R"sql(
            create table if not exists Chain
            (
                TxId     integer primary key, -- Transactions.Id
                BlockId  int     not null,
                BlockNum int     not null,
                Height   int     not null,

                -- AccountUser.Id
                -- ContentPost.Id
                -- ContentVideo.Id
                -- Comment.Id
                Uid       int     null
            );
        )sql");


        _tables.emplace_back(R"sql(
            create table if not exists Last
            (
                -- AccountUser
                -- ContentPost
                -- ContentVideo
                -- ContentDelete
                -- Comment
                -- Blocking
                -- Subscribe
                TxId integer primary key
            );
        )sql");

        // TODO (optimization): create table First

        _tables.emplace_back(R"sql(
            create table if not exists Transactions
            (
                RowId     integer primary key,
                Type      int    not null,
                HashId    int    not null, -- Id of tx hash in Registry table
                Time      int    not null,

                -- AccountUser.AddressId
                -- ContentPost.AddressId
                -- ContentVideo.AddressId
                -- ContentDelete.AddressId
                -- Comment.AddressId
                -- ScorePost.AddressId
                -- ScoreComment.AddressId
                -- Subscribe.AddressId
                -- Blocking.AddressId
                -- Complain.AddressId
                -- Boost.AddressId
                RegId1   int   null,

                -- AccountUser.ReferrerAddressId
                -- ContentPost.RootTxId
                -- ContentVideo.RootTxId
                -- ContentDelete.RootTxId
                -- Comment.RootTxId
                -- ScorePost.ContentRootTxId
                -- ScoreComment.CommentRootTxId
                -- Subscribe.AddressToId
                -- Blocking.AddressToId
                -- Complain.ContentRootTxId
                -- Boost.ContentRootTxId
                -- ModerationFlag.ContentTxId
                RegId2   int   null,

                -- ContentPost.RelayRootTxId
                -- ContentVideo.RelayRootTxId
                -- Comment.ContentRootTxId
                RegId3   int   null,

                -- Comment.ParentRootTxId
                RegId4   int   null,

                -- Comment.AnswerRootTxId
                RegId5   int   null,

                -- ScoreContent.Value
                -- ScoreComment.Value
                -- Complain.Reason
                -- Boost.Amount
                -- ModerationFlag.Reason
                Int1      int    null
            );
        )sql");

        _tables.emplace_back(R"sql(
            -- Registry to handle all strings by id
            -- - BlockHashes
            -- - AddressHashes
            -- - TxHashes
            create table if not exists Registry
            (
                RowId integer primary key,
                String text not null
            );
        )sql");

        _tables.emplace_back(R"sql(
            create table if not exists Lists
            (
                TxId       int not null, -- TxId that List belongs to
                OrderIndex int not null, -- Allowes to use different lists for one tx
                RegId      int not null  -- Entry that list contains
            );
        )sql");

        _tables.emplace_back(R"sql(
            create table if not exists Payload
            (
                TxId integer primary key, -- Transactions.TxId

                -- AccountUser.Lang
                -- ContentPost.Lang
                -- ContentVideo.Lang
                -- ContentDelete.Settings
                -- Comment.Message
                String1 text   null,

                -- AccountUser.Name
                -- ContentPost.Caption
                -- ContentVideo.Caption
                String2 text   null,

                -- AccountUser.Avatar
                -- ContentPost.Message
                -- ContentVideo.Message
                String3 text   null,

                -- AccountUser.About
                -- ContentPost.Tags JSON
                -- ContentVideo.Tags JSON
                String4 text   null,

                -- AccountUser.Url
                -- ContentPost.Images JSON
                -- ContentVideo.Images JSON
                String5 text   null,

                -- AccountUser.Pubkey
                -- ContentPost.Settings JSON
                -- ContentVideo.Settings JSON
                String6 text   null,

                -- AccountUser.Donations JSON
                -- ContentPost.Url
                -- ContentVideo.Url
                String7 text   null,

                Int1    int    null
            );
        )sql");

        _tables.emplace_back(R"sql(
            create table if not exists TxOutputs
            (
                TxId            int    not null, -- Transactions.TxId
                Number          int    not null, -- Number in tx.vout
                AddressId       int    not null, -- Address
                Value           int    not null, -- Amount
                ScriptPubKeyId  int    not null -- Original script
            );
        )sql");

        _tables.emplace_back(R"sql(
            create table if not exists TxInputs
            (
                SpentTxId int not null,
                TxId      int not null,
                Number    int not null
            );
        )sql");

        _tables.emplace_back(R"sql(
            create table if not exists Ratings
            (
                Type   int not null,
                Last   int not null,
                Height int not null,
                Uid    int not null,
                Value  int not null
            );
        )sql");

        _tables.emplace_back(R"sql(
            create table if not exists Balances
            (
                AddressId   integer primary key,
                Value       int not null
            );
        )sql");

        _tables.emplace_back(R"sql(
            create table if not exists BlockingLists
            (
                IdSource int not null,
                IdTarget int not null
            );
        )sql");

        _views.emplace_back(R"sql(
            drop view if exists vTx;
            create view if not exists vTx as
            select
                t.RowId, t.Type, t.HashId, t.Time, t.RegId1, t.RegId2, t.RegId3, t.RegId4, t.RegId5, t.Int1,
                r.String as Hash
            from
                Registry r indexed by Registry_String,
                Transactions t indexed by Transactions_HashId
            where
                t.HashId = r.RowId;

            drop view if exists vTxStr;
            create view if not exists vTxStr as
            select
                t.RowId as RowId,
                (select r.String from Registry r where r.RowId = t.HashId) as Hash,
                (select r.String from Registry r where r.RowId = t.RegId1) as String1,
                (select r.String from Registry r where r.RowId = t.RegId2) as String2,
                (select r.String from Registry r where r.RowId = t.RegId3) as String3,
                (select r.String from Registry r where r.RowId = t.RegId4) as String4,
                (select r.String from Registry r where r.RowId = t.RegId5) as String5
            from Transactions t;

            drop view if exists vTxOutStr;
            create view if not exists vTxOutStr as
            select
                o.TxId as TxId,
                (select r.String from Registry r where r.RowId = o.AddressId) as AddressHash,
                (select r.String from Registry r where r.RowId = o.ScriptPubKeyId) as ScriptPubKey
            from TxOutputs o;
        )sql");

        
        _preProcessing = R"sql(
            
        )sql";


        _indexes = R"sql(
            create index if not exists Chain_Uid_Height on Chain (Uid, Height);
            create index if not exists Chain_Height_Uid on Chain (Height, Uid);
            create index if not exists Chain_Height_BlockId on Chain (Height, BlockId);

            create unique index if not exists Registry_String on Registry (String);

            create unique index if not exists Transactions_HashId on Transactions (HashId);
            create index if not exists Transactions_Type_RegId1_RegId2_RegId3 on Transactions (Type, RegId1, RegId2, RegId3);

            create index if not exists TxInputs_SpentTxId_TxId_Number on TxInputs (SpentTxId, TxId, Number);

            create index if not exists TxOutputs_TxId_Number_AddressId on TxOutputs (TxId, Number, AddressId);

            create index if not exists Lists_TxId_OrderIndex_RegId on Lists (TxId, OrderIndex asc, RegId);

            ------------------------------

            create index if not exists Ratings_Last_Uid_Height on Ratings (Last, Uid, Height);
            create index if not exists Ratings_Height_Last on Ratings (Height, Last);
            create index if not exists Ratings_Type_Uid_Value on Ratings (Type, Uid, Value);
            create index if not exists Ratings_Type_Uid_Last_Height on Ratings (Type, Uid, Last, Height);
            create index if not exists Ratings_Type_Uid_Last_Value on Ratings (Type, Uid, Last, Value);
            create index if not exists Ratings_Type_Uid_Height_Value on Ratings (Type, Uid, Height, Value);

            create index if not exists Payload_String2_nocase_TxId on Payload (String2 collate nocase, TxId);
            create index if not exists Payload_String7 on Payload (String7);
            create index if not exists Payload_String1_TxId on Payload (String1, TxId);

        )sql";

        _postProcessing = R"sql(

        )sql";
    }
}