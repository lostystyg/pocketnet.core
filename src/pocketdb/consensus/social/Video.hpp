// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETCONSENSUS_VIDEO_HPP
#define POCKETCONSENSUS_VIDEO_HPP

#include "pocketdb/ReputationConsensus.h"
#include "pocketdb/consensus/Social.h"
#include "pocketdb/models/dto/Video.h"

namespace PocketConsensus
{
    using namespace std;
    typedef shared_ptr<Video> VideoRef;

    /*******************************************************************************************************************
    *  Video consensus base class
    *******************************************************************************************************************/
    class VideoConsensus : public SocialConsensus<Video>
    {
    public:
        VideoConsensus(int height) : SocialConsensus<Video>(height) {}
        ConsensusValidateResult Validate(const VideoRef& ptx, const PocketBlockRef& block) override
        {
            // Base validation with calling block or mempool check
            if (auto[baseValidate, baseValidateCode] = SocialConsensus::Validate(ptx, block); !baseValidate)
                return {false, baseValidateCode};

            if (ptx->IsEdit())
                return ValidateEdit(ptx);

            return Success;
        }
        ConsensusValidateResult Check(const CTransactionRef& tx, const VideoRef& ptx) override
        {
            if (auto[baseCheck, baseCheckCode] = SocialConsensus::Check(tx, ptx); !baseCheck)
                return {false, baseCheckCode};

            // Check required fields
            if (IsEmpty(ptx->GetAddress())) return {false, SocialConsensusResult_Failed};

            // Repost not allowed
            if (!IsEmpty(ptx->GetRelayTxHash())) return {false, SocialConsensusResult_NotAllowed};

            return Success;
        }

    protected:
        virtual int GetLimitWindow() { return Limitor({1440, 1440}); }
        virtual int64_t GetEditWindow() { return Limitor({1440, 1440}); }
        virtual int64_t GetProLimit() { return Limitor({100, 100}); }
        virtual int64_t GetFullLimit() { return Limitor({30, 30}); }
        virtual int64_t GetTrialLimit() { return Limitor({15, 15}); }
        virtual int64_t GetEditLimit() { return Limitor({5, 5}); }

        ConsensusValidateResult ValidateBlock(const VideoRef& ptx, const PocketBlockRef& block) override
        {

            // Edit
            if (ptx->IsEdit())
                return ValidateEditBlock(ptx, block);

            // ---------------------------------------------------------
            // New

            // Get count from chain
            int count = GetChainCount(ptx);

            // Get count from block
            for (auto& blockTx : *block)
            {
                if (!IsIn(*blockTx->GetType(), {CONTENT_VIDEO}))
                    continue;

                auto blockPtx = static_pointer_cast<Video>(blockTx);
                if (*ptx->GetAddress() == *blockPtx->GetAddress())
                {
                    if (blockPtx->IsEdit())
                        continue;

                    if (*blockPtx->GetHash() == *ptx->GetHash())
                        continue;

                    count += 1;
                }
            }

            return ValidateLimit(ptx, count);
        }
        ConsensusValidateResult ValidateMempool(const VideoRef& ptx) override
        {

            // Edit
            if (ptx->IsEdit())
                return ValidateEditMempool(ptx);

            // ---------------------------------------------------------
            // New

            // Get count from chain
            int count = GetChainCount(ptx);

            // and from mempool
            count += ConsensusRepoInst.CountMempoolVideo(*ptx->GetAddress());

            return ValidateLimit(ptx, count);
        }
        vector<string> GetAddressesForCheckRegistration(const VideoRef& ptx) override
        {
            return {*ptx->GetAddress()};
        }

        virtual int64_t GetLimit(AccountMode mode)
        {
            return mode == AccountMode_Pro
                   ? GetProLimit()
                   : mode == AccountMode_Full
                     ? GetFullLimit()
                     : GetTrialLimit();
        }
        virtual ConsensusValidateResult ValidateEdit(const VideoRef& ptx)
        {

            // First get original post transaction
            auto originalTx = PocketDb::TransRepoInst.GetByHash(*ptx->GetRootTxHash());
            if (!originalTx)
                return {false, SocialConsensusResult_NotFound};

            auto originalPtx = static_pointer_cast<Video>(originalTx);

            // Change type not allowed
            if (*originalPtx->GetType() != *ptx->GetType())
                return {false, SocialConsensusResult_NotAllowed};

            // You are author? Really?
            if (*ptx->GetAddress() != *originalPtx->GetAddress())
                return {false, SocialConsensusResult_ContentEditUnauthorized};

            // Original post edit only 24 hours
            if (!AllowEditWindow(ptx, originalPtx))
                return {false, SocialConsensusResult_ContentEditLimit};

            return make_tuple(true, SocialConsensusResult_Success);
        }
        virtual ConsensusValidateResult ValidateLimit(const VideoRef& ptx, int count)
        {

            auto reputationConsensus = PocketConsensus::ReputationConsensusFactoryInst.Instance(Height);

            auto[mode, reputation, balance] = reputationConsensus->GetAccountInfo(*ptx->GetAddress());
            auto limit = GetLimit(mode);

            if (count >= limit)
                return {false, SocialConsensusResult_ContentLimit};

            return Success;
        }
        virtual int GetChainCount(const VideoRef& ptx)
        {

            return ConsensusRepoInst.CountChainVideoHeight(
                *ptx->GetAddress(),
                Height - GetLimitWindow()
            );
        }
        virtual ConsensusValidateResult ValidateEditBlock(const VideoRef& ptx, const PocketBlockRef& block)
        {

            // Double edit in block not allowed
            for (auto& blockTx : *block)
            {
                if (!IsIn(*blockTx->GetType(), {CONTENT_VIDEO}))
                    continue;

                auto blockPtx = static_pointer_cast<Video>(blockTx);

                if (*blockPtx->GetHash() == *ptx->GetHash())
                    continue;

                if (*ptx->GetRootTxHash() == *blockPtx->GetRootTxHash())
                    return {false, SocialConsensusResult_DoubleContentEdit};
            }

            // Check edit limit
            return ValidateEditOneLimit(ptx);
        }
        virtual ConsensusValidateResult ValidateEditMempool(const VideoRef& ptx)
        {

            if (ConsensusRepoInst.CountMempoolVideoEdit(*ptx->GetAddress(), *ptx->GetRootTxHash()) > 0)
                return {false, SocialConsensusResult_DoubleContentEdit};

            // Check edit limit
            return ValidateEditOneLimit(ptx);
        }
        virtual ConsensusValidateResult ValidateEditOneLimit(const VideoRef& ptx)
        {

            int count = ConsensusRepoInst.CountChainVideoEdit(*ptx->GetAddress(), *ptx->GetRootTxHash());
            if (count >= GetEditLimit())
                return {false, SocialConsensusResult_ContentEditLimit};

            return Success;
        }
        virtual bool AllowEditWindow(const VideoRef& ptx, const VideoRef& originalTx)
        {
            auto[ok, originalTxHeight] = ConsensusRepoInst.GetTransactionHeight(*originalTx->GetHash());
            if (!ok)
                return false;

            return (Height - originalTxHeight) <= GetEditWindow();
        }
    };

    /*******************************************************************************************************************
    *  Start checkpoint at 1324655 block
    *******************************************************************************************************************/
    class VideoConsensus_checkpoint_1324655 : public VideoConsensus
    {
    public:
        VideoConsensus_checkpoint_1324655(int height) : VideoConsensus(height) {}
    protected:
        int64_t GetTrialLimit() override { return Limitor({5, 5}); }
    };

    /*******************************************************************************************************************
    *  Factory for select actual rules version
    *******************************************************************************************************************/
    class VideoConsensusFactory
    {
    private:
        const vector<ConsensusCheckpoint < VideoConsensus>> m_rules = {
            { 0, -1, [](int height) { return make_shared<VideoConsensus>(height); }},
            { 1324655, 0, [](int height) { return make_shared<VideoConsensus_checkpoint_1324655>(height); }},
        };
    public:
        shared_ptr<VideoConsensus> Instance(int height)
        {
            int m_height = (height > 0 ? height : 0);
            return (--upper_bound(m_rules.begin(), m_rules.end(), m_height,
                [&](int target, const ConsensusCheckpoint<VideoConsensus>& itm)
                {
                    return target < itm.Height(Params().NetworkIDString());
                }
            ))->m_func(height);
        }
    };
}

#endif // POCKETCONSENSUS_VIDEO_HPP