// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETDB_MIGRATION
#define POCKETDB_MIGRATION

#include <string>
#include <vector>
#include <memory>

namespace PocketDb
{
    using namespace std;

    class PocketDbMigration
    {
    protected:
        vector<string> _tables;
        vector<string> _views;
        string _indexes;

    public:

        explicit PocketDbMigration() = default;

        vector<string>& Tables() { return _tables; }
        vector<string>& Views() { return _views; }
        string& Indexes() { return _indexes; }
    };

    typedef std::shared_ptr<PocketDbMigration> PocketDbMigrationRef;
}

#endif // POCKETDB_MIGRATION