// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef CORE_BLOCKCHAIN_STORAGE_H
#define CORE_BLOCKCHAIN_STORAGE_H

#include <db_lmdb.h>
#include <types.h>

using namespace Types::Blockchain;

namespace Core
{
    class BlockchainStorage
    {
      protected:
        /**
         * Create new instance of the blockchain storage in the specified path
         *
         * @param db_path
         */
        BlockchainStorage(const std::string &db_path);

      public:
        ~BlockchainStorage();

        /**
         * Checks whether the block with the given hash exists in the database
         *
         * @param block_hash
         * @return
         */
        [[nodiscard]] bool block_exists(const crypto_hash_t &block_hash) const;

        /**
         * Checks whether the block with the given index exists in the database
         *
         * @param block_index
         * @return
         */
        [[nodiscard]] bool block_exists(const uint64_t &block_index) const;

        /**
         * Retrieves the block and transactions within that block using the specified block hash
         *
         * @param block_hash
         * @return
         */
        [[nodiscard]] std::tuple<Error, block_t, std::vector<transaction_t>>
            get_block(const crypto_hash_t &block_hash) const;

        /**
         * Retrieves the block and transactions within that block using the specified block index
         *
         * @param block_height
         * @return
         */
        [[nodiscard]] std::tuple<Error, block_t, std::vector<transaction_t>>
            get_block(const uint64_t &block_index) const;

        /**
         * Retrieve the NEXT closest block hash by timestamp using the specified timestamp
         *
         * @param timestamp
         * @return
         */
        [[nodiscard]] std::tuple<Error, uint64_t, crypto_hash_t>
            get_block_by_timestamp(const uint64_t &timestamp) const;

        /**
         * Retrieve the total number of blocks stored in the database
         *
         * @return
         */
        [[nodiscard]] size_t get_block_count() const;

        /**
         * Retrieve the block hash for the given block index
         *
         * @param block_index
         * @return
         */
        [[nodiscard]] std::tuple<Error, crypto_hash_t> get_block_hash(const uint64_t &block_index) const;

        /**
         * Retrieve the block hash for the given block hash
         *
         * @param block_hash
         * @return
         */
        [[nodiscard]] std::tuple<Error, uint64_t> get_block_index(const crypto_hash_t &block_hash) const;

        /**
         * Retrieves a vector of random transaction outputs from the database
         * that meet or exceed the provided block index (spendable)
         *
         * The randomness is provided by generating a random hash then selecting
         * the first transaction output hash greater than or equal to the random
         * hash supplied. This is repeated until the vector is filled to the
         * requested count of outputs. Then the vector is returned sorted by the
         * transaction output hash
         *
         * @param block_index
         * @param count
         * @return
         */
        [[nodiscard]] std::tuple<Error, std::vector<transaction_output_t>>
            get_random_outputs(uint64_t block_index = 0, size_t count = 1) const;

        /**
         * Retrieves the transaction with the specified hash
         *
         * @param txn_hash
         * @return
         */
        [[nodiscard]] std::tuple<Error, transaction_t, crypto_hash_t>
            get_transaction(const crypto_hash_t &txn_hash) const;

        /**
         * Retrieves the transaction output by its hash
         *
         * @param output_hash
         * @return
         */
        [[nodiscard]] std::tuple<Error, transaction_output_t, uint64_t>
            get_transaction_output(const crypto_hash_t &output_hash) const;

        /**
         * Retrieves the transaction outputs by their hashes
         *
         * If any output cannot be found, the entire routine errors and returns with
         * not results
         *
         * @param output_hashes
         * @return
         */
        [[nodiscard]] std::tuple<Error, std::vector<std::tuple<transaction_output_t, uint64_t>>>
            get_transaction_output(const std::vector<crypto_hash_t> &output_hashes) const;

        /**
         * Retrieves a singleton instance of the class
         *
         * @param db_path
         * @return
         */
        static std::shared_ptr<BlockchainStorage> instance(const std::string &db_path);

        /**
         * Checks if the specified key image exists in the database
         *
         * @param key_image
         * @return
         */
        [[nodiscard]] bool key_image_exists(const crypto_key_image_t &key_image) const;

        /**
         * Checks if any of the specified key images exist in the database
         *
         * If they do exist, the already existing key images are returned
         *
         * @param key_images
         * @return
         */
        [[nodiscard]] bool key_image_exists(const std::vector<crypto_key_image_t> &key_images) const;

        /**
         * Returns the number of transaction outputs in the database
         *
         * @return
         */
        [[nodiscard]] size_t output_count() const;

        /**
         * Checks if the provided transaction output exists in the database
         *
         * @param output_hash
         * @return
         */
        [[nodiscard]] bool output_exists(const crypto_hash_t &output_hash) const;

        /**
         * Saves the block with the transactions specified in the database
         *
         * @param block
         * @param transactions
         * @return
         */
        Error put_block(const block_t &block, const std::vector<transaction_t> &transactions);

        /**
         * Rewinds the database to the given block index
         *
         * @param block_index
         * @return
         */
        Error rewind(const uint64_t &block_index);

        /**
         * Returns whether a transaction exists in the database
         *
         * @param txn_hash
         * @return
         */
        [[nodiscard]] bool transaction_exists(const crypto_hash_t &txn_hash) const;

      private:
        /**
         * Delete a block and it's transactions from the database
         *
         * @param block_index
         * @return
         */
        Error del_block(const uint64_t &block_index);

        /**
         * Delete a key image from the database
         * @param db_tx
         * @param key_image
         * @return
         */
        Error del_key_image(std::unique_ptr<Database::LMDBTransaction> &db_tx, const crypto_key_image_t &key_image);

        /**
         * Delete a transaction from the database
         *
         * @param db_tx
         * @param transaction
         * @return
         */
        Error del_transaction(std::unique_ptr<Database::LMDBTransaction> &db_tx, const transaction_t &transaction);

        /**
         * Delete a transaction output from the database
         *
         * @param db_tx
         * @param output_hash
         * @return
         */
        Error
            del_transaction_output(std::unique_ptr<Database::LMDBTransaction> &db_tx, const crypto_hash_t &output_hash);

        /**
         * Saves the specified key image to the database
         *
         * @param db_tx
         * @param key_image
         * @return
         */
        Error put_key_image(std::unique_ptr<Database::LMDBTransaction> &db_tx, const crypto_key_image_t &key_image);

        /**
         * Saves the specified transaction to the database
         *
         * @param db_tx
         * @param transaction
         * @param block_hash
         * @return
         */
        std::tuple<Error, crypto_hash_t> put_transaction(
            std::unique_ptr<Database::LMDBTransaction> &db_tx,
            const transaction_t &transaction,
            const crypto_hash_t &block_hash);

        /**
         * Saves the specified transaction output to the database
         *
         * @param db_tx
         * @param output
         * @param unlock_block
         * @return
         */
        Error put_transaction_output(
            std::unique_ptr<Database::LMDBTransaction> &db_tx,
            const transaction_output_t &output,
            uint64_t unlock_block);

        std::shared_ptr<Database::LMDB> m_db_env;

        std::shared_ptr<Database::LMDBDatabase> m_blocks, m_block_indexes, m_block_timestamps, m_transactions,
            m_key_images, m_transaction_outputs;

        crypto_hash_t m_id;

        std::mutex write_mutex;
    };
} // namespace Core

#endif // CORE_BLOCKCHAIN_STORAGE_H
