// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETCONSENSUS_MODERATION_FLAG_HPP
#define POCKETCONSENSUS_MODERATION_FLAG_HPP

#include "pocketdb/consensus/Reputation.h"
#include "pocketdb/consensus/Social.h"
#include "pocketdb/models/dto/moderation/ContentFlag.h"

namespace PocketConsensus
{
    using namespace std;
    using namespace PocketDb;
    using namespace PocketConsensus;
    typedef shared_ptr<ModerationFlag> ModerationFlagRef;

    /*******************************************************************************************************************
    *  ModerationFlag consensus base class
    *******************************************************************************************************************/
    class ModerationFlag : public SocialConsensus<ModerationFlag>
    {
    public:
        ModerationFlag(int height) : SocialConsensus<ModerationFlag>(height) {}

        ConsensusValidateResult Validate(const CTransactionRef& tx, const ModerationFlagRef& ptx, const PocketBlockRef& block) override
        {
            // Base validation with calling block or mempool check
            if (auto[baseValidate, baseValidateCode] = SocialConsensus::Validate(tx, ptx, block); !baseValidate)
                return {false, baseValidateCode};

            // Only `Shark` account can flag content
            auto reputationConsensus = ReputationConsensusFactoryInst.Instance(Height);
            if (!reputationConsensus->IsShark(*ptx->GetAddress()))
                return {false, SocialConsensusResult_LowReputation};

            // Target transaction must be a exists and is a content
            if (!ConsensusRepoInst.Exists(ptx->GetContentTxHash(), { CONTENT_POST, CONTENT_ARTICLE, CONTENT_VIDEO, CONTENT_COMMENT, CONTENT_COMMENT_EDIT }, true))
                return {false, SocialConsensusResult_NotFound};

            return Success;
        }

        ConsensusValidateResult Check(const CTransactionRef& tx, const ModerationFlagRef& ptx) override
        {
            return {false, SocialConsensusResult_NotAllowed};
        }

    protected:

        ConsensusValidateResult ValidateBlock(const ModerationFlagRef& ptx, const PocketBlockRef& block) override
        {
            // Check flag from one to one in month
            if (ConsensusRepoInst.CountModerationFlag(*ptx->GetAddress(), *ptx->GetAddressTo(), Height - (int)GetConsensusLimit(ConsensusLimit_moderation_flag_one_to_one_depth), false) > 1)
                return {false, SocialConsensusResult_Duplicate};

            // Count flags in chain
            int count = ConsensusRepoInst.CountModerationFlag(*ptx->GetAddress(), Height - (int)GetConsensusLimit(ConsensusLimit_depth), false);

            // Count flags in block
            for (auto& blockTx : *block)
            {
                if (!TransactionHelper::IsIn(*blockTx->GetType(), { MODERATION_FLAG }) || blockTx->GetHash() == ptx->GetHash())
                    continue;

                auto blockPtx = static_pointer_cast<ModerationFlagRef>(blockTx);
                if (*ptx->GetAddress() == *blockPtx->GetAddress())
                    if (*ptx->GetContentTxHash() == *blockPtx->GetContentTxHash())
                        return {false, SocialConsensusResult_Duplicate};
                    else
                        count += 1;
            }

            // Check limit
            return ValidateLimit(ptx, count);
        }

        ConsensusValidateResult ValidateMempool(const ModerationFlagRef& ptx) override
        {
            // Check flag from one to one in month
            if (ConsensusRepoInst.CountModerationFlag(*ptx->GetAddress(), *ptx->GetAddressTo(), Height - (int)GetConsensusLimit(ConsensusLimit_moderation_flag_one_to_one_depth), true) > 1)
                return {false, SocialConsensusResult_Duplicate};

            // Check limit
            return ValidateLimit(ptx, ConsensusRepoInst.CountModerationFlag(*ptx->GetAddress(), Height - (int)GetConsensusLimit(ConsensusLimit_depth), true));
        }

        vector<string> GetAddressesForCheckRegistration(const ModerationFlagRef& ptx) override
        {
            return { *ptx->GetAddress(), *ptx->GetContentAddressHash() };
        }

        virtual ConsensusValidateResult ValidateLimit(const ModerationFlagRef& ptx, int count)
        {
            if (count >= GetConsensusLimit(ConsensusLimit_moderation_flag_count))
                return {false, SocialConsensusResult_ExceededLimit};

            return Success;
        }
    };

    /*******************************************************************************************************************
    *  Enable ModerationFlag consensus rules
    *******************************************************************************************************************/
    class ModerationFlag_checkpoint_enable : public ModerationFlag
    {
    public:
        ModerationFlag_checkpoint_enable(int height) : ModerationFlag(height) {}
    protected:
        ConsensusValidateResult Check(const CTransactionRef& tx, const ModerationFlagRef& ptx) override
        {
            if (auto[baseCheck, baseCheckCode] = SocialConsensus::Check(tx, ptx); !baseCheck)
                return {false, baseCheckCode};

            // Check required fields
            if (IsEmpty(ptx->GetAddress())) return {false, SocialConsensusResult_Failed};
            if (IsEmpty(ptx->GetContentTxHash())) return {false, SocialConsensusResult_Failed};
            if (IsEmpty(ptx->GetContentAddressHash())) return {false, SocialConsensusResult_Failed};
            if (IsEmpty(ptx->GetReason()) || *ptx->GetReason() < 1 || *ptx->GetReason() > 4) return {false, SocialConsensusResult_Failed};

            return Success;
        }
    };


    /*******************************************************************************************************************
    *  Factory for select actual rules version
    *******************************************************************************************************************/
    class ModerationFlagFactory
    {
    private:
        const vector<ConsensusCheckpoint < ModerationFlag>> m_rules = {
            {       0,     -1, [](int height) { return make_shared<ModerationFlag>(height); }},
            { 9999999,      1, [](int height) { return make_shared<ModerationFlag_checkpoint_enable>(height); }}, // TODO (brangr): !!!!!! set heights 761000 for test
        };
    public:
        shared_ptr<ModerationFlag> Instance(int height)
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

#endif // POCKETCONSENSUS_MODERATION_FLAG_HPP