// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETCONSENSUS_POST_H
#define POCKETCONSENSUS_POST_H

#include "pocketdb/consensus/Social.h"
#include "pocketdb/models/dto/content/Post.h"

namespace PocketConsensus
{
    typedef shared_ptr<Post> PostRef;
    typedef shared_ptr<Content> ContentRef;

    // TODO (aok) (v0.21.0): extract base class Content for Post, Video and ContentDelete
    // Also split Post & Video for extract PostEdit & VideoEdit transactions with base class ContentEdit

    /*******************************************************************************************************************
    *  Post consensus base class
    *******************************************************************************************************************/
    class PostConsensus : public SocialConsensus<Post>
    {
    public:
        PostConsensus() : SocialConsensus<Post>()
        {
            // TODO (limits): set limits
        }

        tuple<bool, SocialConsensusResult> Validate(const CTransactionRef& tx, const PostRef& ptx, const PocketBlockRef& block) override
        {
            // Check if this post relay another
            if (!IsEmpty(ptx->GetRelayTxHash()))
            {
                auto[relayOk, relayTx] = PocketDb::ConsensusRepoInst.GetLastContent(
                    *ptx->GetRelayTxHash(),
                    { CONTENT_POST, CONTENT_VIDEO, CONTENT_ARTICLE, CONTENT_STREAM, CONTENT_AUDIO, CONTENT_DELETE }
                );

                if (!relayOk && !CheckpointRepoInst.IsSocialCheckpoint(*ptx->GetHash(), *ptx->GetType(), ConsensusResult_RelayContentNotFound))
                    return {false, ConsensusResult_RelayContentNotFound};

                if (relayOk)
                {
                    // Repost deleted content not allowed
                    if (*relayTx->GetType() == CONTENT_DELETE)
                        return {false, ConsensusResult_RepostDeletedContent};

                    // Check Blocking
                    if (auto[ok, result] = ValidateBlocking(*relayTx->GetString1(), ptx); !ok)
                        return {false, result};
                }
            }

            // Check payload size
            if (auto[ok, code] = ValidatePayloadSize(ptx); !ok)
                return {false, code};

            if (ptx->IsEdit())
                return ValidateEdit(ptx);

            return SocialConsensus::Validate(tx, ptx, block);
        }
        tuple<bool, SocialConsensusResult> Check(const CTransactionRef& tx, const PostRef& ptx) override
        {
            if (auto[baseCheck, baseCheckCode] = SocialConsensus::Check(tx, ptx); !baseCheck)
                return {false, baseCheckCode};

            // Check required fields
            if (IsEmpty(ptx->GetAddress()))
                return {false, ConsensusResult_Failed};

            return Success;
        }

    protected:
        virtual int64_t GetLimit(AccountMode mode)
        {
            return mode >= AccountMode_Full ? GetConsensusLimit(ConsensusLimit_full_post) : GetConsensusLimit(ConsensusLimit_trial_post);
        }

        tuple<bool, SocialConsensusResult> ValidateBlock(const PostRef& ptx, const PocketBlockRef& block) override
        {
            // Edit posts
            if (ptx->IsEdit())
                return ValidateEditBlock(ptx, block);

            // ---------------------------------------------------------
            // New posts

            // Get count from chain
            int count = GetChainCount(ptx);

            // Get count from block
            for (const auto& blockTx : *block)
            {
                if (!TransactionHelper::IsIn(*blockTx->GetType(), {CONTENT_POST}))
                    continue;

                const auto blockPtx = static_pointer_cast<Post>(blockTx);

                if (*ptx->GetAddress() != *blockPtx->GetAddress())
                    continue;

                if (blockPtx->IsEdit())
                    continue;

                if (*blockPtx->GetHash() == *ptx->GetHash())
                    continue;

                if (AllowBlockLimitTime(ptx, blockPtx))
                    count += 1;
            }

            return ValidateLimit(ptx, count);
        }
        tuple<bool, SocialConsensusResult> ValidateMempool(const PostRef& ptx) override
        {
            // Edit posts
            if (ptx->IsEdit())
                return ValidateEditMempool(ptx);

            // ---------------------------------------------------------
            // New posts

            // Get count from chain
            int count = GetChainCount(ptx);

            // Get count from mempool
            count += ConsensusRepoInst.CountMempoolPost(*ptx->GetAddress());

            return ValidateLimit(ptx, count);
        }
        vector<string> GetAddressesForCheckRegistration(const PostRef& ptx) override
        {
            return {*ptx->GetAddress()};
        }

        virtual tuple<bool, SocialConsensusResult> ValidateEdit(const PostRef& ptx)
        {
            auto[lastContentOk, lastContent] = PocketDb::ConsensusRepoInst.GetLastContent(
                *ptx->GetRootTxHash(),
                { CONTENT_POST, CONTENT_VIDEO, CONTENT_ARTICLE, CONTENT_STREAM, CONTENT_AUDIO, CONTENT_DELETE }
            );
            if (lastContentOk && *lastContent->GetType() != CONTENT_POST)
                return {false, ConsensusResult_NotAllowed};

            // First get original post transaction
            auto[originalTxOk, originalTx] = PocketDb::ConsensusRepoInst.GetFirstContent(*ptx->GetRootTxHash());
            if (!lastContentOk || !originalTxOk)
                return {false, ConsensusResult_NotFound};

            const auto originalPtx = static_pointer_cast<Content>(originalTx);

            // Change type not allowed
            if (*originalTx->GetType() != *ptx->GetType())
                return {false, ConsensusResult_NotAllowed};

            // You are author? Really?
            if (*ptx->GetAddress() != *originalPtx->GetAddress())
                return {false, ConsensusResult_ContentEditUnauthorized};

            // Original post edit only 24 hours
            if (!AllowEditWindow(ptx, originalPtx))
                return {false, ConsensusResult_ContentEditLimit};

            // Check edit limit
            return ValidateEditOneLimit(ptx);
        }
        virtual tuple<bool, SocialConsensusResult> ValidateLimit(const PostRef& ptx, int count)
        {
            auto reputationConsensus = PocketConsensus::ConsensusFactoryInst_Reputation.Instance(Height);
            auto address = ptx->GetAddress();
            auto[mode, reputation, balance] = reputationConsensus->GetAccountMode(*address);
            if (count >= GetLimit(mode))
            {
                if (!CheckpointRepoInst.IsSocialCheckpoint(*ptx->GetHash(), *ptx->GetType(), ConsensusResult_ContentLimit))
                    return {false, ConsensusResult_ContentLimit};
            }

            return Success;
        }
        virtual bool AllowBlockLimitTime(const PostRef& ptx, const PostRef& blockPtx)
        {
            return *blockPtx->GetTime() <= *ptx->GetTime();
        }
        virtual bool AllowEditWindow(const PostRef& ptx, const ContentRef& originalTx)
        {
            return (*ptx->GetTime() - *originalTx->GetTime()) <= GetConsensusLimit(ConsensusLimit_edit_post_depth);
        }
        virtual int GetChainCount(const PostRef& ptx)
        {
            return ConsensusRepoInst.CountChainPostTime(
                *ptx->GetAddress(),
                *ptx->GetTime() - GetConsensusLimit(ConsensusLimit_depth)
            );
        }
        virtual tuple<bool, SocialConsensusResult> ValidateEditBlock(const PostRef& ptx, const PocketBlockRef& block)
        {
            // Double edit in block not allowed
            for (auto& blockTx : *block)
            {
                if (!TransactionHelper::IsIn(*blockTx->GetType(), {CONTENT_POST, CONTENT_DELETE}))
                    continue;

                auto blockPtx = static_pointer_cast<Post>(blockTx);

                if (*blockPtx->GetHash() == *ptx->GetHash())
                    continue;

                if (*ptx->GetRootTxHash() == *blockPtx->GetRootTxHash())
                    return {false, ConsensusResult_DoubleContentEdit};
            }

            // Check edit limit
            return ValidateEditOneLimit(ptx);
        }
        virtual tuple<bool, SocialConsensusResult> ValidateEditMempool(const PostRef& ptx)
        {
            if (ConsensusRepoInst.CountMempoolPostEdit(*ptx->GetAddress(), *ptx->GetRootTxHash()) > 0)
                return {false, ConsensusResult_DoubleContentEdit};

            // Check edit limit
            return ValidateEditOneLimit(ptx);
        }
        virtual tuple<bool, SocialConsensusResult> ValidateEditOneLimit(const PostRef& ptx)
        {
            int count = ConsensusRepoInst.CountChainPostEdit(*ptx->GetAddress(), *ptx->GetRootTxHash());
            if (count >= GetConsensusLimit(ConsensusLimit_post_edit_count))
                return {false, ConsensusResult_ContentEditLimit};

            return Success;
        }
        virtual ConsensusValidateResult ValidatePayloadSize(const PostRef& ptx)
        {
            size_t dataSize =
                (ptx->GetPayloadUrl() ? ptx->GetPayloadUrl()->size() : 0) +
                (ptx->GetPayloadCaption() ? ptx->GetPayloadCaption()->size() : 0) +
                (ptx->GetPayloadMessage() ? ptx->GetPayloadMessage()->size() : 0) +
                (ptx->GetRelayTxHash() ? ptx->GetRelayTxHash()->size() : 0) +
                (ptx->GetPayloadSettings() ? ptx->GetPayloadSettings()->size() : 0) +
                (ptx->GetPayloadLang() ? ptx->GetPayloadLang()->size() : 0);

            if (ptx->GetRootTxHash() && *ptx->GetRootTxHash() != *ptx->GetHash())
                dataSize += ptx->GetRootTxHash()->size();

            if (!IsEmpty(ptx->GetPayloadTags()))
            {
                UniValue tags(UniValue::VARR);
                tags.read(*ptx->GetPayloadTags());
                for (size_t i = 0; i < tags.size(); ++i)
                    dataSize += tags[i].get_str().size();
            }

            if (!IsEmpty(ptx->GetPayloadImages()))
            {
                UniValue images(UniValue::VARR);
                images.read(*ptx->GetPayloadImages());
                for (size_t i = 0; i < images.size(); ++i)
                    dataSize += images[i].get_str().size();
            }

            if (dataSize > (size_t)GetConsensusLimit(ConsensusLimit_max_post_size))
                return {false, ConsensusResult_ContentSizeLimit};

            return Success;
        }
        virtual ConsensusValidateResult ValidateBlocking(const string& contentAddress, const PostRef& ptx)
        {
            return Success;
        }
    };

    /*******************************************************************************************************************
    *  Start checkpoint at 1124000 block
    *******************************************************************************************************************/
    class PostConsensus_checkpoint_1124000 : public PostConsensus
    {
    public:
        PostConsensus_checkpoint_1124000() : PostConsensus() {}
    protected:
        bool AllowBlockLimitTime(const PostRef& ptx, const PostRef& blockPtx) override
        {
            return true;
        }
    };

    /*******************************************************************************************************************
    *  Start checkpoint at 1180000 block
    *******************************************************************************************************************/
    class PostConsensus_checkpoint_1180000 : public PostConsensus_checkpoint_1124000
    {
    public:
        PostConsensus_checkpoint_1180000() : PostConsensus_checkpoint_1124000() {}
        
    protected:
        int GetChainCount(const PostRef& ptx) override
        {
            return ConsensusRepoInst.CountChainPostHeight(
                *ptx->GetAddress(),
                Height - (int) GetConsensusLimit(ConsensusLimit_depth)
            );
        }
        bool AllowEditWindow(const PostRef& ptx, const ContentRef& originalTx) override
        {
            auto[ok, originalTxHeight] = ConsensusRepoInst.GetTransactionHeight(*originalTx->GetHash());
            if (!ok)
                return false;

            return (Height - originalTxHeight) <= GetConsensusLimit(ConsensusLimit_edit_post_depth);
        }
    };

    // Disable scores for blocked accounts
    class PostConsensus_checkpoint_disable_for_blocked : public PostConsensus_checkpoint_1180000
    {
    public:
        PostConsensus_checkpoint_disable_for_blocked() : PostConsensus_checkpoint_1180000() {}

    protected:
        ConsensusValidateResult ValidateBlocking(const string& contentAddress, const PostRef& ptx) override
        {
            auto[existsBlocking, blockingType] = PocketDb::ConsensusRepoInst.GetLastBlockingType(
                contentAddress,
                *ptx->GetAddress()
            );

            if (existsBlocking && blockingType == ACTION_BLOCKING)
                return {false, ConsensusResult_Blocking};

            return Success;
        }
    };


    // ----------------------------------------------------------------------------------------------
    // Factory for select actual rules version
    class PostConsensusFactory : public BaseConsensusFactory<PostConsensus>
    {
    public:
        PostConsensusFactory()
        {
            Checkpoint({       0,     -1, -1, make_shared<PostConsensus>() });
            Checkpoint({ 1124000,     -1, -1, make_shared<PostConsensus_checkpoint_1124000>() });
            Checkpoint({ 1180000,     -1, -1, make_shared<PostConsensus_checkpoint_1180000>() });
            Checkpoint({ 1757000, 953000,  0, make_shared<PostConsensus_checkpoint_disable_for_blocked>() });
        }
    };

    static PostConsensusFactory ConsensusFactoryInst_Post;
}

#endif // POCKETCONSENSUS_POST_H