// Copyright (c) 2018-2022 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETDB_MIGRATION_WEB
#define POCKETDB_MIGRATION_WEB

#include <string>
#include <vector>

#include "pocketdb/migrations/base.h"

namespace PocketDb
{
    class PocketDbWebMigration : public PocketDbMigration
    {
    public:
        PocketDbWebMigration();
    };
}

#endif // POCKETDB_MIGRATION_WEB