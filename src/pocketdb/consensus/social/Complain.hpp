// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETCONSENSUS_COMPLAIN_HPP
#define POCKETCONSENSUS_COMPLAIN_HPP

#include "pocketdb/consensus/Reputation.h"
#include "pocketdb/consensus/Social.h"
#include "pocketdb/models/dto/Complain.h"

namespace PocketConsensus
{
    using namespace std;
    typedef shared_ptr<Complain> ComplainRef;

    /*******************************************************************************************************************
    *  Complain consensus base class
    *******************************************************************************************************************/
    class ComplainConsensus : public SocialConsensus<Complain>
    {
    public:
        ComplainConsensus(int height) : SocialConsensus<Complain>(height) {}
        ConsensusValidateResult Validate(const ComplainRef& ptx, const PocketBlockRef& block) override
        {
            // Base validation with calling block or mempool check
            if (auto[baseValidate, baseValidateCode] = SocialConsensus::Validate(ptx, block); !baseValidate)
                return {false, baseValidateCode};

            // Author or post must be exists
            auto[contentOk, contentTx] = PocketDb::ConsensusRepoInst.GetLastContent(*ptx->GetPostTxHash());
            if (!contentOk)
                return {false, SocialConsensusResult_NotFound};

            // Complain to self
            if (*contentTx->GetString1() == *ptx->GetAddress())
                return {false, SocialConsensusResult_SelfComplain};

            if (*contentTx->GetType() == CONTENT_DELETE)
                return {false, SocialConsensusResult_ComplainDeletedContent};

            // Check double complain
            if (PocketDb::ConsensusRepoInst.ExistsComplain(*ptx->GetHash(), *ptx->GetPostTxHash(), *ptx->GetAddress()))
                return {false, SocialConsensusResult_DoubleComplain};

            return Success;
        }
        ConsensusValidateResult Check(const CTransactionRef& tx, const ComplainRef& ptx) override
        {
            if (auto[baseCheck, baseCheckCode] = SocialConsensus::Check(tx, ptx); !baseCheck)
                return {false, baseCheckCode};

            // Check required fields
            if (IsEmpty(ptx->GetAddress())) return {false, SocialConsensusResult_Failed};
            if (IsEmpty(ptx->GetPostTxHash())) return {false, SocialConsensusResult_Failed};
            if (IsEmpty(ptx->GetReason())) return {false, SocialConsensusResult_Failed};

            return Success;
        }

    protected:
        ConsensusValidateResult ValidateBlock(const ComplainRef& ptx, const PocketBlockRef& block) override
        {
            int count = GetChainCount(ptx);

            for (auto& blockTx : *block)
            {
                if (!IsIn(*blockTx->GetType(), {ACTION_COMPLAIN}))
                    continue;

                if (*blockTx->GetHash() == *ptx->GetHash())
                    continue;

                auto blockPtx = static_pointer_cast<Complain>(blockTx);
                if (*ptx->GetAddress() == *blockPtx->GetAddress())
                {
                    if (CheckBlockLimitTime(ptx, blockPtx))
                        count += 1;

                    // Maybe in block
                    if (*ptx->GetPostTxHash() == *blockPtx->GetPostTxHash())
                    {
                        PocketHelpers::SocialCheckpoints socialCheckpoints;
                        if (!socialCheckpoints.IsCheckpoint(*ptx->GetHash(), *ptx->GetType(), SocialConsensusResult_DoubleComplain))
                            return {false, SocialConsensusResult_DoubleComplain};
                    }
                }
            }

            return ValidateLimit(ptx, count);
        }
        ConsensusValidateResult ValidateMempool(const ComplainRef& ptx) override
        {
            int count = GetChainCount(ptx);
            count += ConsensusRepoInst.CountMempoolComplain(*ptx->GetAddress());
            return ValidateLimit(ptx, count);
        }
        vector<string> GetAddressesForCheckRegistration(const ComplainRef& ptx) override
        {
            return {*ptx->GetAddress()};
        }

        virtual int64_t GetComplainsLimit(AccountMode mode)
        {
            return mode >= AccountMode_Full ? 
            GetConsensusLimit(ConsensusLimit_full_complain) : 
            GetConsensusLimit(ConsensusLimit_trial_complain);
        }
        virtual ConsensusValidateResult ValidateLimit(const ComplainRef& ptx, int count)
        {
            auto reputationConsensus = PocketConsensus::ReputationConsensusFactoryInst.Instance(Height);
            auto[mode, reputation, balance] = reputationConsensus->GetAccountMode(*ptx->GetAddress());
            if (count >= GetComplainsLimit(mode))
                return {false, SocialConsensusResult_ComplainLimit};

            auto minimumReputation = GetConsensusLimit(ConsensusLimit_threshold_reputation);
            if (reputation < minimumReputation)
                return {false, SocialConsensusResult_LowReputation};

            return Success;
        }
        virtual bool CheckBlockLimitTime(const ComplainRef& ptx, const ComplainRef& blockPtx)
        {
            return *blockPtx->GetTime() <= *ptx->GetTime();
        }
        virtual int GetChainCount(const ComplainRef& ptx)
        {
            return ConsensusRepoInst.CountChainComplainTime(
                *ptx->GetAddress(),
                *ptx->GetTime() - GetConsensusLimit(ConsensusLimit_depth)
            );
        }
    };

    /*******************************************************************************************************************
    *  Start checkpoint at 1124000 block
    *******************************************************************************************************************/
    class ComplainConsensus_checkpoint_1124000 : public ComplainConsensus
    {
    public:
        ComplainConsensus_checkpoint_1124000(int height) : ComplainConsensus(height) {}
    protected:
        bool CheckBlockLimitTime(const ComplainRef& ptx, const ComplainRef& blockPtx) override
        {
            return true;
        }
    };

    /*******************************************************************************************************************
    *  Start checkpoint at 1180000 block
    *******************************************************************************************************************/
    class ComplainConsensus_checkpoint_1180000 : public ComplainConsensus_checkpoint_1124000
    {
    public:
        ComplainConsensus_checkpoint_1180000(int height) : ComplainConsensus_checkpoint_1124000(height) {}
    protected:
        int GetChainCount(const ComplainRef& ptx) override
        {
            return ConsensusRepoInst.CountChainComplainHeight(*ptx->GetAddress(), Height - (int) GetConsensusLimit(ConsensusLimit_depth));
        }
    };


    /*******************************************************************************************************************
    *  Factory for select actual rules version
    *******************************************************************************************************************/
    class ComplainConsensusFactory
    {
    private:
        const vector<ConsensusCheckpoint < ComplainConsensus>> m_rules = {
            { 0, -1, [](int height) { return make_shared<ComplainConsensus>(height); }},
            { 1124000, -1, [](int height) { return make_shared<ComplainConsensus_checkpoint_1124000>(height); }},
            { 1180000, 0, [](int height) { return make_shared<ComplainConsensus_checkpoint_1180000>(height); }},
        };
    public:
        shared_ptr<ComplainConsensus> Instance(int height)
        {
            int m_height = (height > 0 ? height : 0);
            return (--upper_bound(m_rules.begin(), m_rules.end(), m_height,
                [&](int target, const ConsensusCheckpoint<ComplainConsensus>& itm)
                {
                    return target < itm.Height(Params().NetworkIDString());
                }
            ))->m_func(height);
        }
    };
}

#endif // POCKETCONSENSUS_COMPLAIN_HPP