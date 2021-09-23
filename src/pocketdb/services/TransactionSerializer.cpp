// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/services/TransactionSerializer.h"

namespace PocketServices
{
    tuple<bool, PocketBlock> TransactionSerializer::DeserializeBlock(CBlock& block, CDataStream& stream)
    {
        // Get Serialized data from stream
        auto pocketData = parseStream(stream);
        return deserializeBlock(block, pocketData);
    }
    tuple<bool, PocketBlock> TransactionSerializer::DeserializeBlock(CBlock& block)
    {
        UniValue fakeData(UniValue::VOBJ);
        return deserializeBlock(block, fakeData);
    }

    tuple<bool, PTransactionRef> TransactionSerializer::DeserializeTransactionRpc(const CTransactionRef& tx, const UniValue& pocketData)
    {
        auto ptx = buildInstanceRpc(tx, pocketData);
        return {ptx != nullptr, ptx};
    }

    tuple<bool, PTransactionRef> TransactionSerializer::DeserializeTransaction(const CTransactionRef& tx, CDataStream& stream)
    {
        auto pocketData = parseStream(stream);
        return deserializeTransaction(tx, pocketData);
    }

    tuple<bool, PTransactionRef> TransactionSerializer::DeserializeTransaction(const CTransactionRef& tx)
    {
        UniValue fakeData(UniValue::VOBJ);
        return deserializeTransaction(tx, fakeData);
    }

    // Serialize protocol compatible with Reindexer
    // It makes sense to serialize only Pocket transactions that contain a payload.
    shared_ptr <UniValue> TransactionSerializer::SerializeBlock(const PocketBlock& block)
    {
        auto result = make_shared<UniValue>(UniValue(UniValue::VOBJ));
        for (const auto& transaction : block)
        {
            auto dataPtr = SerializeTransaction(*transaction);
            if (!dataPtr)
                continue;

            result->pushKV(*transaction->GetHash(), dataPtr->write());
        }

        return result;
    }

    // Serialize protocol compatible with Reindexer
    // It makes sense to serialize only Pocket transactions that contain a payload.
    shared_ptr <UniValue> TransactionSerializer::SerializeTransaction(const Transaction& transaction)
    {
        if (!PocketHelpers::TransactionHelper::IsPocketTransaction(*transaction.GetType()))
            return nullptr;

        auto result = make_shared<UniValue>(UniValue(UniValue::VOBJ));

        auto serializedTransaction = transaction.Serialize();
        auto base64Transaction = EncodeBase64(serializedTransaction->write());

        result->pushKV("t", PocketHelpers::TransactionHelper::ConvertToReindexerTable(transaction));
        result->pushKV("d", base64Transaction);

        return result;
    }


    shared_ptr <Transaction> TransactionSerializer::buildInstance(const CTransactionRef& tx, const UniValue& src)
    {
        PocketTxType txType;
        if (!PocketHelpers::TransactionHelper::IsPocketSupportedTransaction(tx, txType))
            return nullptr;

        shared_ptr <Transaction> ptx = PocketHelpers::TransactionHelper::CreateInstance(txType, tx);
        if (!ptx)
            return nullptr;

        // Build outputs & inputs
        if (!buildOutputs(tx, ptx))
            return nullptr;

        // Deserialize payload if exists
        if (src.exists("d"))
        {
            UniValue txDataSrc(UniValue::VOBJ);
            auto txDataBase64 = src["d"].get_str();
            auto txJson = DecodeBase64(txDataBase64);
            txDataSrc.read(txJson);

            if (src.exists("t") && src["t"].get_str() == "Mempool" && txDataSrc.exists("data"))
            {
                auto txMempoolDataBase64 = txDataSrc["data"].get_str();
                auto txMempoolJson = DecodeBase64(txMempoolDataBase64);
                txDataSrc.read(txMempoolJson);
            }

            ptx->Deserialize(txDataSrc);
            ptx->DeserializePayload(txDataSrc, tx);
        }

        return ptx;
    }

    shared_ptr <Transaction> TransactionSerializer::buildInstanceRpc(const CTransactionRef& tx, const UniValue& src)
    {
        PocketTxType txType;
        if (!PocketHelpers::TransactionHelper::IsPocketSupportedTransaction(tx, txType))
            return nullptr;

        shared_ptr <Transaction> ptx = PocketHelpers::TransactionHelper::CreateInstance(txType, tx);
        if (!ptx)
            return nullptr;

        // Build outputs & inputs
        if (!buildOutputs(tx, ptx))
            return nullptr;

        ptx->DeserializeRpc(src, tx);
        return ptx;
    }


    bool TransactionSerializer::buildOutputs(const CTransactionRef& tx, shared_ptr <Transaction>& ptx)
    {
        // indexing Outputs
        for (size_t i = 0; i < tx->vout.size(); i++)
        {
            const CTxOut& txout = tx->vout[i];

            txnouttype type;
            std::vector <CTxDestination> vDest;
            int nRequired;
            if (ExtractDestinations(txout.scriptPubKey, type, vDest, nRequired))
            {
                for (const auto& dest : vDest)
                {
                    auto out = make_shared<TransactionOutput>();
                    out->SetTxHash(tx->GetHash().GetHex());
                    out->SetNumber((int) i);
                    out->SetAddressHash(EncodeDestination(dest));
                    out->SetValue(txout.nValue);
                    out->SetScriptPubKey(HexStr(txout.scriptPubKey));

                    ptx->Outputs().push_back(out);
                }
            }
        }

        return !ptx->Outputs().empty();
    }

    UniValue TransactionSerializer::parseStream(CDataStream& stream)
    {
        // Prepare source data - old format (Json)
        UniValue pocketData(UniValue::VOBJ);
        if (!stream.empty())
        {
            // TODO (brangr) (v0.21.0): speed up protocol
            string src;
            stream >> src;
            pocketData.read(src);
        }

        return pocketData;
    }


    tuple<bool, PocketBlock> TransactionSerializer::deserializeBlock(CBlock& block, UniValue& pocketData)
    {
        // Restore pocket transaction instance
        PocketBlock pocketBlock;
        for (const auto& tx : block.vtx)
        {
            auto txHash = tx->GetHash().GetHex();

            UniValue entry(UniValue::VOBJ);
            if (pocketData.exists(txHash))
            {
                auto entrySrc = pocketData[txHash];

                try
                {
                    entry.read(entrySrc.get_str());
                }
                catch (std::exception& ex)
                {
                    LogPrintf("Error deserialize transaction: %s: %s\n", txHash, ex.what());
                }
            }

            if (auto[ok, ptx] = deserializeTransaction(tx, entry); ok && ptx)
                pocketBlock.push_back(ptx);
        }

        // TODO (brangr): check deserialize success
        //bool resultCheck = pocketBlock.size() == (block.vtx.size() - 1);
        return {true, pocketBlock};
    }

    tuple<bool, shared_ptr<Transaction>> TransactionSerializer::deserializeTransaction(const CTransactionRef& tx, UniValue& pocketData)
    {
        auto ptx = buildInstance(tx, pocketData);
        return {ptx != nullptr, ptx};
    }
}