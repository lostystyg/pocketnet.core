// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETTX_COMMENT_EDIT_H
#define POCKETTX_COMMENT_EDIT_H

#include "pocketdb/models/dto/Comment.h"

namespace PocketTx
{
    class CommentEdit : public Comment
    {
    public:
        CommentEdit(const string& hash, int64_t time);
        CommentEdit(const std::shared_ptr<const CTransaction>& tx);
    };
} // namespace PocketTx

#endif //POCKETTX_COMMENT_EDIT_H
