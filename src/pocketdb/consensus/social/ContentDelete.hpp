// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETCONSENSUS_CONTENT_DELETE_HPP
#define POCKETCONSENSUS_CONTENT_DELETE_HPP

#include "pocketdb/consensus/Social.h"
#include "pocketdb/models/dto/content/ContentDelete.h"

namespace PocketConsensus
{
    typedef shared_ptr<ContentDelete> ContentDeleteRef;

    /*******************************************************************************************************************
    *  ContentDelete consensus base class
    *******************************************************************************************************************/
    class ContentDeleteConsensus : public SocialConsensus<ContentDelete>
    {
    public:
        ContentDeleteConsensus() : SocialConsensus<ContentDelete>()
        {
            // TODO (limits): set limits
        }

        ConsensusValidateResult Validate(const CTransactionRef& tx, const ContentDeleteRef& ptx, const PocketBlockRef& block) override
        {
            // Actual content not deleted
            auto[ok, actuallTx] = ConsensusRepoInst.GetLastContent(
                *ptx->GetRootTxHash(),
                { CONTENT_POST, CONTENT_VIDEO, CONTENT_ARTICLE, CONTENT_STREAM, CONTENT_AUDIO, CONTENT_COLLECTION, CONTENT_DELETE }
            );

            if (!ok)
                return {false, SocialConsensusResult_NotFound};

            if (*actuallTx->GetType() == TxType::CONTENT_DELETE)
                return {false, SocialConsensusResult_ContentDeleteDouble};

            // TODO (aok): convert to Content base class
            // You are author? Really?
            if (*ptx->GetAddress() != *actuallTx->GetString1())
                return {false, SocialConsensusResult_ContentDeleteUnauthorized};

            return SocialConsensus::Validate(tx, ptx, block);
        }
        ConsensusValidateResult Check(const CTransactionRef& tx, const ContentDeleteRef& ptx) override
        {
            if (auto[baseCheck, baseCheckCode] = SocialConsensus::Check(tx, ptx); !baseCheck)
                return {false, baseCheckCode};

            // Check required fields
            if (IsEmpty(ptx->GetAddress())) return {false, SocialConsensusResult_Failed};
            if (IsEmpty(ptx->GetRootTxHash())) return {false, SocialConsensusResult_Failed};

            return Success;
        }

    protected:
        ConsensusValidateResult ValidateBlock(const ContentDeleteRef& ptx, const PocketBlockRef& block) override
        {
            for (auto& blockTx : *block)
            {
                if (!TransactionHelper::IsIn(*blockTx->GetType(), {CONTENT_POST, CONTENT_VIDEO, CONTENT_STREAM, CONTENT_AUDIO, CONTENT_ARTICLE, CONTENT_DELETE}))
                    continue;

                if (*blockTx->GetHash() == *ptx->GetHash())
                    continue;

                // TODO (aok): convert to content base class
                if (*ptx->GetRootTxHash() == *blockTx->GetString2())
                    return {false, SocialConsensusResult_ContentDeleteDouble};
            }

            return Success;
        }
        ConsensusValidateResult ValidateMempool(const ContentDeleteRef& ptx) override
        {
            if (ConsensusRepoInst.CountMempoolContentDelete(*ptx->GetAddress(), *ptx->GetRootTxHash()) > 0)
                return {false, SocialConsensusResult_ContentDeleteDouble};

            return Success;
        }
        vector<string> GetAddressesForCheckRegistration(const ContentDeleteRef& ptx) override
        {
            return {*ptx->GetString1()};
        }
    };


    // ----------------------------------------------------------------------------------------------
    // Factory for select actual rules version
    class ContentDeleteConsensusFactory : public BaseConsensusFactory<ContentDeleteConsensus>
    {
    public:
        ContentDeleteConsensusFactory()
        {
            Checkpoint({ 0, 0, 0, make_shared<ContentDeleteConsensus>() });
        }
    };

    static ContentDeleteConsensusFactory ConsensusFactoryInst_ContentDelete;
}

#endif // POCKETCONSENSUS_CONTENT_DELETE_HPP