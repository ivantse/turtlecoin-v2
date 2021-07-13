// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef CORE_STAKING_ENGINE_H
#define CORE_STAKING_ENGINE_H

#include <db_lmdb.h>
#include <types.h>

using namespace Types::Staking;

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

        /**
         * Adds a new candidate to the database
         *
         * @param candidate
         * @return
         */
        Error add_candidate(const candidate_node_t &candidate);

        /**
         * Adds a new staker to the database
         *
         * @param staker
         * @return
         */
        Error add_staker(const staker_t &staker);

        /**
         * Checks whether the specified candidate exists in the database
         *
         * @param candidate_key
         * @return
         */
        [[nodiscard]] bool candidate_exists(const crypto_public_key_t &candidate_key) const;

        /**
         * Calculates the election seed from the given last blocks presented
         *
         * @param last_round_blocks the hashes of the blocks in the last round
         * @return [seed, seed_uint, evenness]
         */
        static std::tuple<crypto_public_key_t, uint256_t, bool>
            calculate_election_seed(const std::vector<crypto_hash_t> &last_round_blocks) ;

        /**
         * Deletes the candidate from the database
         *
         * @param candidate_key
         * @return
         */
        Error delete_candidate(const crypto_public_key_t &candidate_key);

        /**
         * Deletes the staker from the database
         *
         * @param staker_id
         * @return
         */
        Error delete_staker(const crypto_hash_t &staker_id);

        /**
         * Retrieves the candidate record for the given candidate key
         *
         * @param candidate_key
         * @return [found, candidate_record]
         */
        [[nodiscard]] std::tuple<Error, candidate_node_t> get_candidate(const crypto_public_key_t &candidate_key) const;

        /**
         * Retrieves all of the active stakes for the given candidate
         *
         * @param candidate_key
         * @return
         */
        [[nodiscard]] std::vector<stake_t> get_candidate_stakes(const crypto_public_key_t &candidate_key) const;

        /**
         * Retrieves the number of votes for a specific candidate key
         *
         * Returns 0 if the candidate is unknown
         *
         * @param candidate_key
         * @return
         */
        [[nodiscard]] uint64_t get_candidate_votes(const crypto_public_key_t &candidate_key) const;

        /**
         * Retrieves the keys for all candidates in the database
         *
         * @return
         */
        [[nodiscard]] std::vector<crypto_public_key_t> get_candidates() const;

        /**
         * Retrieves the staker record for the given staker key
         *
         * @param staker_key
         * @return [found, staker_record]
         */
        [[nodiscard]] std::tuple<Error, staker_t> get_staker(const crypto_hash_t &staker_key) const;

        /**
         * Retrieves all tally of all of a staker's votes for a particular candidate
         *
         * @param staker_id
         * @param candidate_key
         * @return
         */
        [[nodiscard]] uint64_t
            get_staker_candidate_votes(const crypto_hash_t &staker_id, const crypto_public_key_t &candidate_key) const;

        /**
         * Retrieves the keys for all stakers in the database
         *
         * @return
         */
        [[nodiscard]] std::vector<crypto_hash_t> get_stakers() const;

        /**
         * Retrieve all of the stakes that the given staker has placed
         *
         * @param staker_id
         * @return
         */
        [[nodiscard]] std::map<crypto_public_key_t, std::vector<stake_t>> get_staker_stakes(const crypto_hash_t &staker_id) const;

        /**
         * Retrieves a singleton instance of the class
         *
         * @param db_path
         * @return
         */
        static std::shared_ptr<StakingEngine> instance(const std::string &db_path);

        /**
         * Recall a the stake with the given parameters
         *
         * @param staker
         * @param candidate_key
         * @param stake
         * @return
         */
        Error recall_stake(const staker_t &staker, const crypto_public_key_t &candidate_key, const uint64_t &stake);

        /**
         * Records a stake with the given parameters
         *
         * @param staker
         * @param candidate_key
         * @param stake
         * @return
         */
        Error record_stake(const staker_t &staker, const crypto_public_key_t &candidate_key, const uint64_t &stake);

        /**
         * Performs the election process to determine the producers and validators for the next
         * round of blocks given the previous round of block hashes and returns, at maximum, the
         * requested number of elected producers and validators
         *
         * @param last_round_blocks
         * @param maximum_keys
         * @return
         */
        [[nodiscard]] std::tuple<std::vector<crypto_public_key_t>, std::vector<crypto_public_key_t>> run_election(
            const std::vector<crypto_hash_t> &last_round_blocks,
            size_t maximum_keys = Configuration::Consensus::ELECTOR_TARGET_COUNT) const;

      private:
        std::shared_ptr<Database::LMDB> m_db_env;

        std::shared_ptr<Database::LMDBDatabase> m_db_candidates, m_db_stakers, m_db_stakes;

        crypto_hash_t m_id;

        std::mutex write_mutex;
    };
} // namespace Core

#endif // CORE_STAKING_ENGINE_H
