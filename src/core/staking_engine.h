// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef CORE_STAKING_ENGINE_H
#define CORE_STAKING_ENGINE_H

#include <db_lmdb.h>
#include <types.h>

using namespace Types::Staking;
using namespace Types::Blockchain;

namespace Core
{
    /**
     * Represents the core staking engine
     */
    class StakingEngine
    {
      protected:
        /**
         * Creates a new instance of the Staking Engine with the database in the
         * provided path
         * @param db_path the path in which to store the database
         */
        StakingEngine(const std::string &db_path);

      public:
        ~StakingEngine();

        Error add_stake(const committed_stake_transaction_t &tx);

        Error del_stake(
            const committed_recall_stake_transaction_t &recall_tx,
            const stake_refund_transaction_t &refund_tx);

        /**
         * Retrieves a singleton instance of the class
         *
         * @param db_path
         * @return
         */
        static std::shared_ptr<StakingEngine> instance(const std::string &db_path);

        Error process_staker_tx(const staker_transaction_t &tx);

      private:
        std::shared_ptr<Database::LMDB> m_db_env;

        std::shared_ptr<Database::LMDBDatabase> m_db_candidates, m_db_stakes;

        crypto_hash_t m_id;

        std::mutex write_mutex;
    };
} // namespace Core

#endif // CORE_STAKING_ENGINE_H
