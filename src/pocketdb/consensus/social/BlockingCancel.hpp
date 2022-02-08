// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETCONSENSUS_BLOCKINGCANCEL_HPP
#define POCKETCONSENSUS_BLOCKINGCANCEL_HPP

#include "pocketdb/consensus/Social.h"
#include "pocketdb/models/dto/BlockingCancel.h"

namespace PocketConsensus
{
    using namespace std;
    typedef shared_ptr<BlockingCancel> BlockingCancelRef;

    /*******************************************************************************************************************
    *  BlockingCancel consensus base class
    *******************************************************************************************************************/
    class BlockingCancelConsensus : public SocialConsensus<BlockingCancel>
    {
    public:
        BlockingCancelConsensus(int height) : SocialConsensus<BlockingCancel>(height) {}
        ConsensusValidateResult Validate(const CTransactionRef& tx, const BlockingCancelRef& ptx, const PocketBlockRef& block) override
        {
            // Base validation with calling block or mempool check
            if (auto[baseValidate, baseValidateCode] = SocialConsensus::Validate(tx, ptx, block); !baseValidate)
                return {false, baseValidateCode};

            if (auto[existsBlocking, blockingType] = PocketDb::ConsensusRepoInst.GetLastBlockingType(
                    *ptx->GetAddress(),
                    *ptx->GetAddressTo()
                ); !existsBlocking || blockingType != ACTION_BLOCKING)
            {
                if (!CheckpointRepoInst.IsSocialCheckpoint(*ptx->GetHash(), *ptx->GetType(), SocialConsensusResult_InvalidBlocking))
                    return {false, SocialConsensusResult_InvalidBlocking};
            }

            return Success;
        }
        ConsensusValidateResult Check(const CTransactionRef& tx, const BlockingCancelRef& ptx) override
        {
            if (auto[baseCheck, baseCheckCode] = SocialConsensus::Check(tx, ptx); !baseCheck)
                return {false, baseCheckCode};

            // Check required fields
            if (IsEmpty(ptx->GetAddress())) return {false, SocialConsensusResult_Failed};
            if (IsEmpty(ptx->GetAddressTo())) return {false, SocialConsensusResult_Failed};

            // Blocking self
            if (*ptx->GetAddress() == *ptx->GetAddressTo())
                return {false, SocialConsensusResult_SelfBlocking};

            return Success;
        }

    protected:
        ConsensusValidateResult ValidateBlock(const BlockingCancelRef& ptx, const PocketBlockRef& block) override
        {

            // Only one transaction (address -> addressTo) allowed in block
            for (auto& blockTx : *block)
            {
                if (!TransactionHelper::IsIn(*blockTx->GetType(), {ACTION_BLOCKING, ACTION_BLOCKING_CANCEL}))
                    continue;

                if (*blockTx->GetHash() == *ptx->GetHash())
                    continue;

                auto blockPtx = static_pointer_cast<BlockingCancel>(blockTx);
                if (*ptx->GetAddress() == *blockPtx->GetAddress() && *ptx->GetAddressTo() == *blockPtx->GetAddressTo())
                    return {false, SocialConsensusResult_ManyTransactions};
            }

            return Success;
        }
        ConsensusValidateResult ValidateMempool(const BlockingCancelRef& ptx) override
        {
            if (ConsensusRepoInst.CountMempoolBlocking(*ptx->GetAddress(), *ptx->GetAddressTo()) > 0)
                return {false, SocialConsensusResult_ManyTransactions};

            return Success;
        }
        vector<pair<string, TxType>> GetAddressesForCheckRegistration(const BlockingCancelRef& ptx) override
        {
            // TODO (losty-video): probabl not only user?
            return {{*ptx->GetAddress(), ACCOUNT_USER}, {*ptx->GetAddressTo(), ACCOUNT_USER}};
        }
    };

    /*******************************************************************************************************************
    *  Factory for select actual rules version
    *******************************************************************************************************************/
    class BlockingCancelConsensusFactory
    {
    protected:
        const vector<ConsensusCheckpoint < BlockingCancelConsensus>> m_rules = {
            { 0, 0, [](int height) { return make_shared<BlockingCancelConsensus>(height); }},
        };
    public:
        shared_ptr<BlockingCancelConsensus> Instance(int height)
        {
            int m_height = (height > 0 ? height : 0);
            return (--upper_bound(m_rules.begin(), m_rules.end(), m_height,
                [&](int target, const ConsensusCheckpoint<BlockingCancelConsensus>& itm)
                {
                    return target < itm.Height(Params().NetworkIDString());
                }
            ))->m_func(height);
        }
    };
}

#endif // POCKETCONSENSUS_BLOCKINGCANCEL_HPP