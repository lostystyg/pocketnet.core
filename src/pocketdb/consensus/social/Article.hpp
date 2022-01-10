// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETCONSENSUS_ARTICLE_H
#define POCKETCONSENSUS_ARTICLE_H

#include "pocketdb/consensus/Social.h"
#include "pocketdb/models/dto/Article.h"

namespace PocketConsensus
{
    using namespace std;
    typedef shared_ptr<Article> ArticleRef;
    typedef shared_ptr<Content> ContentRef;

    /*******************************************************************************************************************
    *  Article consensus base class
    *******************************************************************************************************************/
    class ArticleConsensus : public SocialConsensus<Article>
    {
    public:
        ArticleConsensus(int height) : SocialConsensus<Article>(height) {}
        tuple<bool, SocialConsensusResult> Validate(const CTransactionRef& tx, const ArticleRef& ptx, const PocketBlockRef& block) override
        {
            // Base validation with calling block or mempool check
            if (auto[baseValidate, baseValidateCode] = SocialConsensus::Validate(tx, ptx, block); !baseValidate)
                return {false, baseValidateCode};

            // Check if this Article relay another
            if (!IsEmpty(ptx->GetRelayTxHash()))
            {
                auto[relayOk, relayTx] = PocketDb::ConsensusRepoInst.GetLastContent(
                    *ptx->GetRelayTxHash(),
                    { CONTENT_POST, CONTENT_VIDEO, CONTENT_DELETE }
                );

                if (!relayOk && !CheckpointRepoInst.IsSocialCheckpoint(*ptx->GetHash(), *ptx->GetType(), SocialConsensusResult_RelayContentNotFound))
                    return {false, SocialConsensusResult_RelayContentNotFound};

                if (relayOk && *relayTx->GetType() == CONTENT_DELETE)
                    return {false, SocialConsensusResult_RepostDeletedContent};
            }

            // Check payload size
            if (auto[ok, code] = ValidatePayloadSize(ptx); !ok)
                return {false, code};

            if (ptx->IsEdit())
                return ValidateEdit(ptx);

            return Success;
        }
        tuple<bool, SocialConsensusResult> Check(const CTransactionRef& tx, const ArticleRef& ptx) override
        {
            if (auto[baseCheck, baseCheckCode] = SocialConsensus::Check(tx, ptx); !baseCheck)
                return {false, baseCheckCode};

            // Check required fields
            if (IsEmpty(ptx->GetAddress())) return {false, SocialConsensusResult_Failed};

            return Success;
        }

    protected:
        virtual int64_t GetLimit(AccountMode mode)
        {
            return mode >= AccountMode_Full ? GetConsensusLimit(ConsensusLimit_full_post) : GetConsensusLimit(ConsensusLimit_trial_post);
        }

        tuple<bool, SocialConsensusResult> ValidateBlock(const ArticleRef& ptx, const PocketBlockRef& block) override
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

                // if (AllowBlockLimitTime(ptx, blockPtx))
                //     count += 1;
            }

            return ValidateLimit(ptx, count);
        }
        tuple<bool, SocialConsensusResult> ValidateMempool(const ArticleRef& ptx) override
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
        vector<string> GetAddressesForCheckRegistration(const ArticleRef& ptx) override
        {
            return {*ptx->GetAddress()};
        }

        virtual tuple<bool, SocialConsensusResult> ValidateEdit(const ArticleRef& ptx)
        {
            auto[lastContentOk, lastContent] = PocketDb::ConsensusRepoInst.GetLastContent(
                *ptx->GetRootTxHash(),
                { CONTENT_POST, CONTENT_VIDEO, CONTENT_DELETE }
            );
            if (lastContentOk && *lastContent->GetType() != CONTENT_POST)
                return {false, SocialConsensusResult_NotAllowed};

            // First get original post transaction
            auto[originalTxOk, originalTx] = PocketDb::ConsensusRepoInst.GetFirstContent(*ptx->GetRootTxHash());
            if (!lastContentOk || !originalTxOk)
                return {false, SocialConsensusResult_NotFound};

            const auto originalPtx = static_pointer_cast<Content>(originalTx);

            // Change type not allowed
            if (*originalTx->GetType() != *ptx->GetType())
                return {false, SocialConsensusResult_NotAllowed};

            // You are author? Really?
            if (*ptx->GetAddress() != *originalPtx->GetAddress())
                return {false, SocialConsensusResult_ContentEditUnauthorized};

            // Original post edit only 24 hours
            if (!AllowEditWindow(ptx, originalPtx))
                return {false, SocialConsensusResult_ContentEditLimit};

            // Check edit limit
            return ValidateEditOneLimit(ptx);
        }
        virtual tuple<bool, SocialConsensusResult> ValidateLimit(const ArticleRef& ptx, int count)
        {
            auto reputationConsensus = PocketConsensus::ReputationConsensusFactoryInst.Instance(Height);
            auto[mode, reputation, balance] = reputationConsensus->GetAccountMode(*ptx->GetAddress());
            if (count >= GetLimit(mode))
            {
                if (!CheckpointRepoInst.IsSocialCheckpoint(*ptx->GetHash(), *ptx->GetType(), SocialConsensusResult_ContentLimit))
                    return {false, SocialConsensusResult_ContentLimit};
            }

            return Success;
        }
        virtual bool AllowBlockLimitTime(const ArticleRef& ptx, const ArticleRef& blockPtx)
        {
            return *blockPtx->GetTime() <= *ptx->GetTime();
        }
        virtual bool AllowEditWindow(const ArticleRef& ptx, const ContentRef& originalTx)
        {
            return (*ptx->GetTime() - *originalTx->GetTime()) <= GetConsensusLimit(ConsensusLimit_edit_post_depth);
        }
        virtual int GetChainCount(const ArticleRef& ptx)
        {
            return ConsensusRepoInst.CountChainPostTime(
                *ptx->GetAddress(),
                *ptx->GetTime() - GetConsensusLimit(ConsensusLimit_depth)
            );
        }
        virtual tuple<bool, SocialConsensusResult> ValidateEditBlock(const ArticleRef& ptx, const PocketBlockRef& block)
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
                    return {false, SocialConsensusResult_DoubleContentEdit};
            }

            // Check edit limit
            return ValidateEditOneLimit(ptx);
        }
        virtual tuple<bool, SocialConsensusResult> ValidateEditMempool(const ArticleRef& ptx)
        {
            if (ConsensusRepoInst.CountMempoolPostEdit(*ptx->GetAddress(), *ptx->GetRootTxHash()) > 0)
                return {false, SocialConsensusResult_DoubleContentEdit};

            // Check edit limit
            return ValidateEditOneLimit(ptx);
        }
        virtual tuple<bool, SocialConsensusResult> ValidateEditOneLimit(const ArticleRef& ptx)
        {
            int count = ConsensusRepoInst.CountChainPostEdit(*ptx->GetAddress(), *ptx->GetRootTxHash());
            if (count >= GetConsensusLimit(ConsensusLimit_post_edit_count))
                return {false, SocialConsensusResult_ContentEditLimit};

            return Success;
        }
        virtual ConsensusValidateResult ValidatePayloadSize(const ArticleRef& ptx)
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
                return {false, SocialConsensusResult_ContentSizeLimit};

            return Success;
        }
    };

    /*******************************************************************************************************************
    *  Factory for select actual rules version
    *******************************************************************************************************************/
    class ArticleConsensusFactory
    {
    protected:
        const vector<ConsensusCheckpoint < ArticleConsensus>> m_rules = {
            { 0, 0, [](int height) { return make_shared<ArticleConsensus>(height); }},
        };
    public:
        shared_ptr<ArticleConsensus> Instance(int height)
        {
            int m_height = (height > 0 ? height : 0);
            return (--upper_bound(m_rules.begin(), m_rules.end(), m_height,
                [&](int target, const ConsensusCheckpoint<ArticleConsensus>& itm)
                {
                    return target < itm.Height(Params().NetworkIDString());
                }
            ))->m_func(height);
        }
    };
} // namespace PocketConsensus

#endif // POCKETCONSENSUS_ARTICLE_H