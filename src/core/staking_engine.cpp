// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "staking_engine.h"

static ThreadSafeMap<crypto_hash_t, std::shared_ptr<Core::StakingEngine>> instances;

namespace Core
{
    StakingEngine::StakingEngine(const std::string &db_path)
    {
        m_id = Crypto::Hashing::sha3(db_path.data(), db_path.size());

        m_db_env = Database::LMDB::instance(db_path);

        m_db_candidates = m_db_env->open_database("candidates");

        m_db_stakes = m_db_env->open_database("stakes", MDB_CREATE | MDB_DUPSORT);
    }

    StakingEngine::~StakingEngine()
    {
        if (instances.contains(m_id))
        {
            instances.erase(m_id);
        }
    }

    Error StakingEngine::add_stake(const committed_stake_transaction_t &tx)
    {
        std::scoped_lock lock(write_mutex);

        /**
         * Version 1 == staking for candidacy
         * Version 2 == staking a candidate (vote)
         */

        if (tx.version == 1)
        {
            if (m_db_candidates->exists(tx.candidate_public_key))
            {
                return MAKE_ERROR(STAKING_CANDIDATE_ALREADY_EXISTS);
            }

            if (tx.stake_amount != Configuration::Consensus::REQUIRED_CANDIDACY_AMOUNT)
            {
                return MAKE_ERROR(STAKING_CANDIDATE_AMOUNT_INVALID);
            }

            candidate_node_t candidate(
                tx.candidate_public_key, tx.staker_public_view_key, tx.staker_public_spend_key, tx.stake_amount);

            auto error = m_db_candidates->put(tx.candidate_public_key, candidate);

            if (error)
            {
                return error;
            }
        }
        else if (tx.version == 2)
        {
            if (!m_db_candidates->exists(tx.candidate_public_key))
            {
                return MAKE_ERROR(STAKING_CANDIDATE_NOT_FOUND);
            }

            if (tx.stake_amount < Configuration::Consensus::MINIMUM_STAKE_AMOUNT)
            {
                return MAKE_ERROR(STAKING_STAKE_AMOUNT);
            }

            // TODO: finish
        }
        else
        {
            return MAKE_ERROR(TX_INVALID_VERSION);
        }

        return MAKE_ERROR(SUCCESS);
    }

    Error StakingEngine::del_stake(
        const committed_recall_stake_transaction_t &recall_tx,
        const stake_refund_transaction_t &refund_tx)
    {
        // TODO: finish

        return MAKE_ERROR(SUCCESS);
    }

    std::shared_ptr<StakingEngine> StakingEngine::instance(const std::string &db_path)
    {
        const auto id = Crypto::Hashing::sha3(db_path.data(), db_path.size());

        if (!instances.contains(id))
        {
            auto db = new StakingEngine(db_path);

            auto ptr = std::shared_ptr<StakingEngine>(db);

            instances.insert(id, ptr);
        }

        return instances.at(id);
    }

    Error StakingEngine::process_staker_tx(const staker_transaction_t &tx)
    {
        // TODO: finish

        return MAKE_ERROR(SUCCESS);
    }
} // namespace Core
