// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 Bitcoin developers
// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETCONSENSUS_SCOREPOST_HPP
#define POCKETCONSENSUS_SCOREPOST_HPP

#include "pocketdb/consensus/Base.hpp"

namespace PocketConsensus
{
    /*******************************************************************************************************************
    *
    *  ScorePost consensus base class
    *
    *******************************************************************************************************************/
    class ScorePostConsensus : public BaseConsensus
    {
    protected:
    public:
        ScorePostConsensus(int height) : BaseConsensus(height) {}
    };


    /*******************************************************************************************************************
    *
    *  Start checkpoint
    *
    *******************************************************************************************************************/
    class ScorePostConsensus_checkpoint_0 : public ScorePostConsensus
    {
    protected:
    public:

        ScorePostConsensus_checkpoint_0(int height) : ScorePostConsensus(height) {}

    }; // class ScorePostConsensus_checkpoint_0


    /*******************************************************************************************************************
    *
    *  Consensus checkpoint at 1 block
    *
    *******************************************************************************************************************/
    class ScorePostConsensus_checkpoint_1 : public ScorePostConsensus_checkpoint_0
    {
    protected:
        int CheckpointHeight() override { return 1; }
    public:
        ScorePostConsensus_checkpoint_1(int height) : ScorePostConsensus_checkpoint_0(height) {}
    };


    /*******************************************************************************************************************
    *
    *  Factory for select actual rules version
    *  Каждая новая перегрузка добавляет новый функционал, поддерживающийся с некоторым условием - например высота
    *
    *******************************************************************************************************************/
    class ScorePostConsensusFactory
    {
    private:
        inline static std::vector<std::pair<int, std::function<ScorePostConsensus *(int height)>>> m_rules
        {
            {1, [](int height) { return new ScorePostConsensus_checkpoint_1(height); }},
            {0, [](int height) { return new ScorePostConsensus_checkpoint_0(height); }},
        };
    public:
        shared_ptr <ScorePostConsensus> Instance(int height)
        {
            for (const auto& rule : m_rules)
                if (height >= rule.first)
                    return shared_ptr<ScorePostConsensus>(rule.second(height));
        }
    };
}

#endif // POCKETCONSENSUS_SCOREPOST_HPP