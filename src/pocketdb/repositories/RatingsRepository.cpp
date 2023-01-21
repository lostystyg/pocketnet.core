// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/repositories/RatingsRepository.h"

namespace PocketDb
{
    void RatingsRepository::InsertRatings(shared_ptr<vector<Rating>> ratings)
    {
        for (const auto& rating: *ratings)
        {
            switch (*rating.GetType())
            {
            case RatingType::ACCOUNT_LIKERS:
            case RatingType::ACCOUNT_LIKERS_POST:
            case RatingType::ACCOUNT_LIKERS_COMMENT_ROOT:
            case RatingType::ACCOUNT_LIKERS_COMMENT_ANSWER:
            case RatingType::ACCOUNT_DISLIKERS_COMMENT_ANSWER:
                InsertLiker(rating);
                break;
            default:
                InsertRating(rating);
                break;
            }
        }
    }

    bool RatingsRepository::ExistsLiker(int addressId, int likerId, const vector<RatingType>& types)
    {
        bool result = false;
        
        string sql = R"sql(
            select 1
            from Ratings indexed by Ratings_Type_Id_Value
            where Type in ( )sql" + join(vector<string>(types.size(), "?"), ",") + R"sql( )
                and Id = ?
                and Value = ?
        )sql";

        TryTransactionStep(__func__, [&]()
        {
            auto stmt = SetupSqlStatement(sql);

            stmt->Bind(types, addressId, likerId);

            if (stmt->Step() == SQLITE_ROW)
                result = true;
        });

        return result;
    }

    void RatingsRepository::InsertRating(const Rating& rating)
    {
        TryTransactionStep(__func__, [&]()
        {
            // Insert new Last record
            auto stmt = SetupSqlStatement(R"sql(
                INSERT OR FAIL INTO Ratings (
                    Type,
                    Last,
                    Height,
                    Id,
                    Value
                ) SELECT ?,1,?,?,
                    ifnull((
                        select r.Value
                        from Ratings r indexed by Ratings_Type_Id_Last_Height
                        where r.Type = ?
                            and r.Last = 1
                            and r.Id = ?
                            and r.Height < ?
                        limit 1
                    ), 0) + ?
            )sql");

            stmt->Bind(
                *rating.GetType(),
                rating.GetHeight(),
                rating.GetId(),
                *rating.GetType(),
                rating.GetId(),
                rating.GetHeight(),
                rating.GetValue()
                );

            TryStepStatement(stmt);

            // Clear old Last record
            auto stmtUpdate = SetupSqlStatement(R"sql(
                update Ratings indexed by Ratings_Type_Id_Last_Height
                  set Last = 0
                where Type = ?
                  and Last = 1
                  and Id = ?
                  and Height < ?
            )sql");

            stmtUpdate->Bind(*rating.GetType(), rating.GetId(), rating.GetHeight());
            TryStepStatement(stmtUpdate);
        });
    }

    void RatingsRepository::InsertLiker(const Rating& rating)
    {
        TryTransactionStep(__func__, [&]()
        {
            auto stmtInsert = SetupSqlStatement(R"sql(
                insert or fail into Ratings (
                    Type,
                    Last,
                    Height,
                    Id,
                    Value
                ) values ( ?,1,?,?,? )
            )sql");

            stmtInsert->Bind(*rating.GetType(), rating.GetHeight(), rating.GetId(), rating.GetValue());
            TryStepStatement(stmtInsert);
        });
    }
}
