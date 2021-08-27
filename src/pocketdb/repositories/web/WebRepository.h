// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETDB_WEBREPOSITORY_H
#define POCKETDB_WEBREPOSITORY_H

#include "pocketdb/helpers/TransactionHelper.h"
#include "pocketdb/repositories/BaseRepository.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <timedata.h>

namespace PocketDb
{
using std::runtime_error;
using boost::algorithm::join;
using boost::adaptors::transformed;

using namespace PocketTx;
using namespace PocketHelpers;

class WebRepository : public BaseRepository
{
public:
    explicit WebRepository(SQLiteDatabase& db) : BaseRepository(db) {}

    void Init() override;
    void Destroy() override;

    UniValue GetUserAddress(string& name);
    UniValue GetAddressesRegistrationDates(vector<string>& addresses);
    UniValue GetAddressInfo(int count);
    UniValue GetAccountState(const string& address);
    UniValue GetCommentsByPost(const std::string& postHash, const std::string& parentHash, const std::string& addressHash);
    UniValue GetCommentsByIds(string& addressHash, vector<string>& commentHashes);
    UniValue GetLastComments(int count, int height, std::string lang = "");
    UniValue GetPostScores(vector<string>& postHashes, string& address);
    UniValue GetPageScores(vector<string>& commentHashes, string& addressHash);
    map<string, UniValue> GetUserProfile(vector<string>& addresses, bool shortForm = true, int option = 0);
    map<string, UniValue> GetSubscribesAddresses(vector<string>& addresses);
    map<string, UniValue> GetSubscribersAddresses(vector<string>& addresses);
    map<string, UniValue> GetBlockingToAddresses(vector<string>& addresses);
    map<string, UniValue> GetContentsData(vector<string>& txids);
    //map<string, UniValue> GetContents(std::map<std::string, param>& conditions, std::optional<int> &counttotal);
    //map<string, UniValue> GetContents(std::map<std::string, param>& conditions);
    map<string, UniValue> GetContents(int nHeight, std::string start_txid, int countOut, std::string lang, std::vector<string> tags, std::vector<int> contentTypes, std::vector<string> txidsExcluded, std::vector<string> adrsExcluded, std::vector<string> tagsExcluded, std::string address);

private:
    UniValue ParseCommentRow(sqlite3_stmt* stmt);

};

typedef std::shared_ptr<WebRepository> WebRepositoryRef;

} // namespace PocketDb

#endif // POCKETDB_WEBREPOSITORY_H

