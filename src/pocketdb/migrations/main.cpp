#include "pocketdb/migrations/main.h"

namespace PocketDb
{
    PocketDbMainMigration::PocketDbMainMigration() : PocketDbMigration()
    {
        _tables.emplace_back(R"sql(
            create table if not exists Transactions
            (
                Type      int    not null,
                Hash      text   not null primary key,
                Time      int    not null,

                BlockHash text   null,
                BlockNum  int    null,
                Height    int    null,

                -- AccountUser
                -- ContentPost
                -- ContentVideo
                -- ContentDelete
                -- Comment
                -- Blocking
                -- Subscribe
                Last      int    not null default 0,

                -- AccountUser.Id
                -- ContentPost.Id
                -- ContentVideo.Id
                -- Comment.Id
                Id        int    null,

                -- AccountUser.AddressHash
                -- ContentPost.AddressHash
                -- ContentVideo.AddressHash
                -- ContentDelete.AddressHash
                -- Comment.AddressHash
                -- ScorePost.AddressHash
                -- ScoreComment.AddressHash
                -- Subscribe.AddressHash
                -- Blocking.AddressHash
                -- Complain.AddressHash
                String1   text   null,

                -- AccountUser.ReferrerAddressHash
                -- ContentPost.RootTxHash
                -- ContentVideo.RootTxHash
                -- ContentDelete.RootTxHash
                -- Comment.RootTxHash
                -- ScorePost.PostTxHash
                -- ScoreComment.CommentTxHash
                -- Subscribe.AddressToHash
                -- Blocking.AddressToHash
                -- Complain.PostTxHash
                String2   text   null,

                -- ContentPost.RelayTxHash
                -- ContentVideo.RelayTxHash
                -- Comment.PostTxHash
                String3   text   null,

                -- Comment.ParentTxHash
                String4   text   null,

                -- Comment.AnswerTxHash
                String5   text   null,

                -- ScoreContent.Value
                -- ScoreComment.Value
                -- Complain.Reason
                Int1      int    null
            );
        )sql");

        _tables.emplace_back(R"sql(
            create table if not exists Payload
            (
                TxHash  text   primary key, -- Transactions.Hash

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

                -- Comment.Donate
                Int1    int    null
            );
        )sql");

        _tables.emplace_back(R"sql(
            create table if not exists TxOutputs
            (
                TxHash          text   not null, -- Transactions.Hash
                TxHeight        int    null,     -- Transactions.Height
                Number          int    not null, -- Number in tx.vout
                AddressHash     text   not null, -- Address
                Value           int    not null, -- Amount
                ScriptPubKey    text   not null, -- Original script
                SpentHeight     int    null,     -- Where spent
                SpentTxHash     text   null,     -- Who spent
                primary key (TxHash, Number, AddressHash)
            );
        )sql");

        _tables.emplace_back(R"sql(
            create table if not exists Ratings
            (
                Type   int not null,
                Last   int not null,
                Height int not null,
                Id     int not null,
                Value  int not null,
                primary key (Type, Height, Id, Value)
            );
        )sql");

        _tables.emplace_back(R"sql(
            create table if not exists Balances
            (
                AddressHash     text    not null,
                Last            int     not null,
                Height          int     not null,
                Value           int     not null,
                primary key (AddressHash, Height)
            );
        )sql");

        _indexes = R"sql(
            create index if not exists Transactions_Id on Transactions (Id);
            create index if not exists Transactions_Id_Last on Transactions (Id, Last);
            create index if not exists Transactions_Hash_Height on Transactions (Hash, Height);
            create index if not exists Transactions_Height_Type on Transactions (Height, Type);
            create index if not exists Transactions_Type_Last_String1_Height on Transactions (Type, Last, String1, Height);
            create index if not exists Transactions_Type_Last_String2_Height on Transactions (Type, Last, String2, Height);
            create index if not exists Transactions_Type_Last_String3_Height on Transactions (Type, Last, String3, Height);
            create index if not exists Transactions_Type_Last_String4_Height on Transactions (Type, Last, String4, Height);
            create index if not exists Transactions_Type_Last_String1_String2_Height on Transactions (Type, Last, String1, String2, Height);
            create index if not exists Transactions_Type_Last_Height_String5_String1 on Transactions (Type, Last, Height, String5, String1);
            create index if not exists Transactions_Type_String1_String2_Height on Transactions (Type, String1, String2, Height);
            create index if not exists Transactions_Type_String1_Height_Time_Int1 on Transactions (Type, String1, Height, Time, Int1);
            create index if not exists Transactions_String1_Last_Height on Transactions (String1, Last, Height);
            create index if not exists Transactions_Last_Id_Height on Transactions (Last, Id, Height);
            create index if not exists Transactions_Time_Type_Height on Transactions (Time, Type, Height);
            create index if not exists Transactions_Type_Time_Height on Transactions (Type, Time, Height);
            create index if not exists Transactions_BlockHash on Transactions (BlockHash);
            create index if not exists Transactions_Type_Last_Height_Time_String3_String4 on Transactions (Type, Last, Height, Time, String3, String4);
            create index if not exists Transactions_Height_Time on Transactions (Height, Time);

            create index if not exists TxOutputs_SpentHeight_AddressHash on TxOutputs (SpentHeight, AddressHash);
            create index if not exists TxOutputs_TxHeight_AddressHash on TxOutputs (TxHeight, AddressHash);
            create index if not exists TxOutputs_SpentTxHash on TxOutputs (SpentTxHash);

            create index if not exists Ratings_Last on Ratings (Last);
            create index if not exists Ratings_Height_Last on Ratings (Height, Last);
            create index if not exists Ratings_Type_Id_Value on Ratings (Type, Id, Value);
            create index if not exists Ratings_Type_Id_Last_Height on Ratings (Type, Id, Last, Height);

            create index if not exists Payload_String2 on Payload (String2);
            create index if not exists Payload_String7 on Payload (String7);
            create index if not exists Payload_String1_TxHash on Payload (String1, TxHash);

            create index if not exists Balances_Height on Balances (Height);
            create index if not exists Balances_AddressHash_Height_Last on Balances (AddressHash, Height, Last);
            create index if not exists Balances_Last_Value on Balances (Last, Value);
            create index if not exists Balances_AddressHash_Last on Balances (AddressHash, Last);
        )sql";
    }
}