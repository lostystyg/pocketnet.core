// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETCONSENSUS_CONTENT_DELETE_HPP
#define POCKETCONSENSUS_CONTENT_DELETE_HPP

#include "pocketdb/consensus/Social.h"
#include "pocketdb/models/dto/ContentDelete.h"

namespace PocketConsensus
{
    using namespace std;
    typedef shared_ptr<ContentDelete> ContentDeleteRef;

    /*******************************************************************************************************************
    *  ContentDelete consensus base class
    *******************************************************************************************************************/
    class ContentDeleteConsensus : public SocialConsensus<ContentDelete>
    {
    public:
        ContentDeleteConsensus(int height) : SocialConsensus<ContentDelete>(height) {}
        ConsensusValidateResult Validate(const ContentDeleteRef& ptx, const PocketBlockRef& block) override
        {
            // Base validation with calling block or mempool check
            if (auto[baseValidate, baseValidateCode] = SocialConsensus::Validate(ptx, block); !baseValidate)
                return {false, baseValidateCode};

            // Actual content not deleted
            if (auto[ok, actuallTx] = ConsensusRepoInst.GetLastContent(*ptx->GetRootTxHash());
                !ok || *actuallTx->GetType() == PocketTxType::CONTENT_DELETE)
                return {false, SocialConsensusResult_ContentDeleteDouble};

            // Original content exists
            auto originalTx = PocketDb::TransRepoInst.GetByHash(*ptx->GetRootTxHash());
            if (!originalTx)
                return {false, SocialConsensusResult_NotFound};

            auto originalPtx = static_pointer_cast<ContentDelete>(originalTx);

            // You are author? Really?
            if (*ptx->GetAddress() != *originalPtx->GetAddress())
                return {false, SocialConsensusResult_ContentDeleteUnauthorized};

            return Success;
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
                if (!IsIn(*blockTx->GetType(), {CONTENT_DELETE}))
                    continue;

                if (*blockTx->GetHash() == *ptx->GetHash())
                    continue;

                auto blockPtx = static_pointer_cast<ContentDelete>(blockTx);

                if (*ptx->GetRootTxHash() == *blockPtx->GetRootTxHash())
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

    /*******************************************************************************************************************
    *  Factory for select actual rules version
    *******************************************************************************************************************/
    class ContentDeleteConsensusFactory
    {
    private:
        const vector<ConsensusCheckpoint < ContentDeleteConsensus>> m_rules = {
            { 0, 0, [](int height) { return make_shared<ContentDeleteConsensus>(height); }},
        };
    public:
        shared_ptr<ContentDeleteConsensus> Instance(int height)
        {
            int m_height = (height > 0 ? height : 0);
            return (--upper_bound(m_rules.begin(), m_rules.end(), m_height,
                [&](int target, const ConsensusCheckpoint<ContentDeleteConsensus>& itm)
                {
                    return target < itm.Height(Params().NetworkIDString());
                }
            ))->m_func(height);
        }
    };
}

#endif // POCKETCONSENSUS_CONTENT_DELETE_HPP