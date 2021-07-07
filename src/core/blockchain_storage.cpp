// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "blockchain_storage.h"

static std::shared_ptr<Core::BlockchainStorage> blockchain_storage_instance;

namespace Core
{
    BlockchainStorage::BlockchainStorage(const std::string &db_path)
    {
        m_db_env = Database::LMDB::getInstance(db_path, 0, 0600, 16, 8);

        m_blocks = m_db_env->open_database("blocks");

        m_block_indexes = m_db_env->open_database("block_indexes");

        m_block_timestamps = m_db_env->open_database("block_timestamps");

        m_transactions = m_db_env->open_database("transactions");

        m_key_images = m_db_env->open_database("key_images");

        m_transaction_outputs = m_db_env->open_database("transaction_outputs");

        m_transaction_block_hashes = m_db_env->open_database("transaction_block_hashes");
    }

    std::shared_ptr<BlockchainStorage> BlockchainStorage::getInstance(const std::string &db_path)
    {
        if (!blockchain_storage_instance)
        {
            blockchain_storage_instance = std::make_shared<BlockchainStorage>(db_path);
        }

        return blockchain_storage_instance;
    }

    bool BlockchainStorage::block_exists(const crypto_hash_t &block_hash) const
    {
        return m_blocks->exists(block_hash);
    }

    bool BlockchainStorage::block_exists(const uint64_t &block_index) const
    {
        return m_block_indexes->exists(block_index);
    }

    std::tuple<Error, Types::Blockchain::block_t, std::vector<Types::Blockchain::transaction_t>>
        BlockchainStorage::get_block(const crypto_hash_t &block_hash) const
    {
        // go get the block
        const auto [error, block] = m_blocks->get<crypto_hash_t, Types::Blockchain::block_t>(block_hash);

        if (error)
        {
            return {MAKE_ERROR(DB_BLOCK_NOT_FOUND), {}, {}};
        }

        std::vector<Types::Blockchain::transaction_t> transactions;

        // loop through the transactions in the block and retrieve them
        for (const auto &txn : block.transactions)
        {
            // retrieve the transaction
            const auto [txn_error, transaction, txn_block_hash] = get_transaction(txn);

            if (txn_error)
            {
                return {MAKE_ERROR(DB_TRANSACTION_NOT_FOUND), {}, {}};
            }

            transactions.push_back(transaction);
        }

        return {MAKE_ERROR(SUCCESS), block, transactions};
    }

    std::tuple<Error, Types::Blockchain::block_t, std::vector<Types::Blockchain::transaction_t>>
        BlockchainStorage::get_block(const uint64_t &block_index) const
    {
        // go get the block
        const auto [error, block_hash] = m_block_indexes->get<crypto_hash_t>(block_index);

        if (error)
        {
            return {MAKE_ERROR(DB_BLOCK_NOT_FOUND), {}, {}};
        }

        return get_block(block_hash);
    }

    std::tuple<Error, uint64_t, crypto_hash_t>
        BlockchainStorage::get_block_by_timestamp(const uint64_t &timestamp) const
    {
        auto txn = m_block_timestamps->transaction(true);

        auto cursor = txn->cursor();

        // go get the next closest (higher) timestamp information
        const auto [error, result_timestamp, block_hash] = cursor->get<crypto_hash_t>(timestamp, MDB_SET_RANGE);

        if (error)
        {
            return {MAKE_ERROR(DB_BLOCK_NOT_FOUND), 0, {}};
        }

        return {error, result_timestamp, block_hash};
    }

    size_t BlockchainStorage::get_block_count() const
    {
        return m_blocks->count();
    }

    std::tuple<Error, crypto_hash_t> BlockchainStorage::get_block_hash(const uint64_t &block_index) const
    {
        // go get the block hash from the indexes
        const auto [error, block_hash] = m_block_indexes->get<crypto_hash_t>(block_index);

        if (error)
        {
            return {MAKE_ERROR(DB_BLOCK_NOT_FOUND), {}};
        }

        return {error, block_hash};
    }

    std::tuple<Error, uint64_t> BlockchainStorage::get_block_index(const crypto_hash_t &block_hash) const
    {
        // go get the block
        const auto [error, block] = m_blocks->get<crypto_hash_t, Types::Blockchain::block_t>(block_hash);

        if (error)
        {
            return {MAKE_ERROR(DB_BLOCK_NOT_FOUND), 0};
        }

        return {error, block.block_index};
    }

    std::tuple<Error, std::vector<Types::Blockchain::transaction_output_t>>
        BlockchainStorage::get_random_outputs(size_t count) const
    {
        std::vector<Types::Blockchain::transaction_output_t> results;

        if (m_transaction_outputs->count() < count)
        {
            return {
                MAKE_ERROR_MSG(DB_TRANSACTION_OUTPUT_NOT_FOUND, "Not enough transaction outputs to complete request."),
                {}};
        }

        auto txn = m_transaction_outputs->transaction(true);

        auto cursor = txn->cursor();

        while (results.size() < count)
        {
            const auto random_hash = Crypto::random_hash();

            const auto [error, key, value] =
                cursor->get<crypto_hash_t, Types::Blockchain::transaction_output_t>(random_hash, MDB_SET_RANGE);

            if (error == LMDB_NOTFOUND || value.hash() != key)
            {
                continue;
            }

            if (std::find(results.begin(), results.end(), value) == results.end())
            {
                results.push_back(value);
            }
        }

        std::sort(results.begin(), results.end());

        return {MAKE_ERROR(SUCCESS), results};
    }

    std::tuple<Error, Types::Blockchain::transaction_t, crypto_hash_t>
        BlockchainStorage::get_transaction(const crypto_hash_t &txn_hash) const
    {
        // go get the transaction
        const auto [error, txn_data] = m_transactions->get(txn_hash);

        if (error)
        {
            return {MAKE_ERROR(DB_TRANSACTION_NOT_FOUND), {}, {}};
        }

        // go get the block hash the transaction is contained within
        const auto [txn_error, block_hash] = m_transaction_block_hashes->get<crypto_hash_t>(txn_hash);

        if (txn_error)
        {
            return {MAKE_ERROR(DB_BLOCK_NOT_FOUND), {}, {}};
        }

        deserializer_t reader(txn_data);

        // figure out what type of transaction it is
        const auto type = reader.varint<uint64_t>(true);

        // depending on the transaction type, we'll return the proper structure
        switch (type)
        {
            case Types::Blockchain::TransactionType::GENESIS:
                return {MAKE_ERROR(SUCCESS), Types::Blockchain::genesis_transaction_t(reader), block_hash};
            case Types::Blockchain::TransactionType::STAKER_REWARD:
                return {MAKE_ERROR(SUCCESS), Types::Blockchain::staker_reward_transaction_t(reader), block_hash};
            case Types::Blockchain::TransactionType::NORMAL:
                return {MAKE_ERROR(SUCCESS), Types::Blockchain::committed_normal_transaction_t(reader), block_hash};
            case Types::Blockchain::TransactionType::STAKE:
                return {MAKE_ERROR(SUCCESS), Types::Blockchain::committed_stake_transaction_t(reader), block_hash};
            case Types::Blockchain::TransactionType::RECALL_STAKE:
                return {
                    MAKE_ERROR(SUCCESS), Types::Blockchain::committed_recall_stake_transaction_t(reader), block_hash};
            case Types::Blockchain::TransactionType::STAKE_REFUND:
                return {MAKE_ERROR(SUCCESS), Types::Blockchain::stake_refund_transaction_t(reader), block_hash};
            default:
                return {MAKE_ERROR(UNKNOWN_TRANSACTION_TYPE), {}, block_hash};
        }
    }

    std::tuple<Error, Types::Blockchain::transaction_output_t>
        BlockchainStorage::get_transaction_output(const crypto_hash_t &output_hash) const
    {
        const auto [error, output] = m_transaction_outputs->get<crypto_hash_t>(output_hash);

        if (error)
        {
            return {MAKE_ERROR(DB_TRANSACTION_OUTPUT_NOT_FOUND), {}};
        }

        return {MAKE_ERROR(SUCCESS), output};
    }

    std::tuple<Error, std::vector<Types::Blockchain::transaction_output_t>>
        BlockchainStorage::get_transaction_output(const std::vector<crypto_hash_t> &output_hashes) const
    {
        std::vector<Types::Blockchain::transaction_output_t> results;

        for (const auto &output_hash : output_hashes)
        {
            const auto [error, output] = get_transaction_output(output_hash);

            if (error)
            {
                return {error, {}};
            }

            results.push_back(output);
        }

        return {MAKE_ERROR(SUCCESS), results};
    }

    bool BlockchainStorage::key_image_exists(const crypto_key_image_t &key_image) const
    {
        return m_key_images->exists(key_image);
    }

    std::map<crypto_key_image_t, bool>
        BlockchainStorage::key_image_exists(const std::vector<crypto_key_image_t> &key_images) const
    {
        auto txn = m_key_images->transaction(true);

        std::map<crypto_key_image_t, bool> results;

        // loop through the requested key images
        for (const auto &key_image : key_images)
        {
            // check to see if the key image exists
            const auto exists = txn->exists(key_image);

            results.insert({key_image, exists});
        }

        return results;
    }

    size_t BlockchainStorage::output_count() const
    {
        return m_transaction_outputs->count();
    }

    Error BlockchainStorage::put_block(
        const Types::Blockchain::block_t &block,
        const std::vector<Types::Blockchain::transaction_t> &transactions)
    {
        /**
         * Sanity check transaction order before write
         */
        {
            // verify that the number of transactions match what is expected
            if (transactions.size() != block.transactions.size())
            {
                return MAKE_ERROR(BLOCK_TXN_MISMATCH);
            }

            // dump the transaction hashes into two vectors so that we can easily compare them
            std::vector<crypto_hash_t> block_tx_hashes, txn_hashes;

            for (const auto &tx : block.transactions)
            {
                block_tx_hashes.push_back(tx);
            }

            for (const auto &tx : transactions)
            {
                std::visit([&txn_hashes](auto &&arg) { txn_hashes.push_back(arg.hash()); }, tx);
            }

            // hash the vectors to get a result that we can match against
            const auto block_hashes = Crypto::Hashing::sha3(block_tx_hashes);

            const auto tx_hashes = Crypto::Hashing::sha3(txn_hashes);

            /**
             * Compare the resulting hashes and if they do not match, then kick back out.
             * The reason that we do this is that it guarantees that the order of the transactions
             * are processed in is the same at each node so that the global indexes match across
             * multiple nodes
             */
            if (block_hashes != tx_hashes)
            {
                return MAKE_ERROR(BLOCK_TXN_ORDER);
            }
        }

        std::scoped_lock lock(write_mutex);

        const auto block_hash = block.hash();

    try_again:

        auto db_tx = m_db_env->transaction();

        // Push the block reward transaction into the database
        {
            auto [error, txn_hash] =
                std::visit([this, &db_tx](auto &&arg) { return put_transaction(db_tx, arg); }, block.reward_tx);

            MDB_CHECK_TXN_EXPAND(error, m_db_env, db_tx, try_again);

            if (error)
            {
                return error;
            }

            auto txn_error = put_transaction_block_hash(db_tx, txn_hash, block_hash);

            MDB_CHECK_TXN_EXPAND(txn_error, m_db_env, db_tx, try_again);

            if (txn_error)
            {
                return txn_error;
            }
        }

        // loop through the individual transactions in the block and push them into the database
        for (const auto &transaction : transactions)
        {
            auto [error, txn_hash] = put_transaction(db_tx, transaction);

            MDB_CHECK_TXN_EXPAND(error, m_db_env, db_tx, try_again);

            if (error)
            {
                return error;
            }

            auto txn_error = put_transaction_block_hash(db_tx, txn_hash, block_hash);

            MDB_CHECK_TXN_EXPAND(txn_error, m_db_env, db_tx, try_again);

            if (txn_error)
            {
                return txn_error;
            }
        }

        // push the block itself into the database
        {
            db_tx->set_database(m_blocks);

            auto error = db_tx->put(block_hash, block.serialize());

            MDB_CHECK_TXN_EXPAND(error, m_db_env, db_tx, try_again);

            if (error)
            {
                return error;
            }
        }

        // push the block index into the database for easy retrieval later
        {
            db_tx->set_database(m_block_indexes);

            auto error = db_tx->put(block.block_index, block_hash);

            MDB_CHECK_TXN_EXPAND(error, m_db_env, db_tx, try_again);

            if (error)
            {
                return error;
            }
        }

        // push the block timestamp into the database for easy retrieval later
        {
            db_tx->set_database(m_block_timestamps);

            auto error = db_tx->put(block.timestamp, block_hash);

            MDB_CHECK_TXN_EXPAND(error, m_db_env, db_tx, try_again);

            if (error)
            {
                return error;
            }
        }

        auto error = db_tx->commit();

        MDB_CHECK_TXN_EXPAND(error, m_db_env, db_tx, try_again);

        return error;
    }

    Error BlockchainStorage::put_key_image(
        std::unique_ptr<Database::LMDBTransaction> &db_tx,
        const crypto_key_image_t &key_image)
    {
        db_tx->set_database(m_key_images);

        return db_tx->put<crypto_key_image_t, std::vector<uint8_t>>(key_image, {});
    }

    std::tuple<Error, crypto_hash_t> BlockchainStorage::put_transaction(
        std::unique_ptr<Database::LMDBTransaction> &db_tx,
        const Types::Blockchain::transaction_t &transaction)
    {
        db_tx->set_database(m_transactions);

        crypto_hash_t txn_hash;

        /**
         * The reason that this looks so convoluted is because of the use
         * of std::variant for the different types of transactions
         */
        {
            auto error = std::visit(
                [this, &db_tx, &txn_hash](auto &&arg)
                {
                    using T = std::decay_t<decltype(arg)>;

                    {
                        txn_hash = arg.hash();

                        // push the transaction itself into the database
                        auto error = db_tx->put(txn_hash, arg.serialize());

                        if (error)
                        {
                            return error;
                        }
                    }

                    // handle the key images on each type of transaction
                    if constexpr (
                        std::is_same_v<
                            T,
                            Types::Blockchain::
                                committed_normal_transaction_t> || std::is_same_v<T, Types::Blockchain::committed_stake_transaction_t> || std::is_same_v<T, Types::Blockchain::committed_recall_stake_transaction_t>)
                    {
                        // loop through the key images in the transaction and push them into the database
                        for (const auto &key_image : arg.key_images)
                        {
                            auto error = put_key_image(db_tx, key_image);

                            if (error)
                            {
                                return error;
                            }
                        }
                    }

                    return MAKE_ERROR(SUCCESS);
                },
                transaction);

            if (error)
            {
                return {error, txn_hash};
            }
        }

        /**
         * The reason that this looks so convoluted is because of the use
         * of std::variant for the different types of transactions
         */
        {
            auto error = std::visit(
                [this, &db_tx](auto &&arg)
                {
                    using T = std::decay_t<decltype(arg)>;

                    // handle the different types of transactions
                    if constexpr (
                        std::is_same_v<
                            T,
                            Types::Blockchain::
                                committed_normal_transaction_t> || std::is_same_v<T, Types::Blockchain::committed_stake_transaction_t> || std::is_same_v<T, Types::Blockchain::committed_recall_stake_transaction_t> || std::is_same_v<T, Types::Blockchain::genesis_transaction_t>)
                    {
                        const auto txn_hash = arg.hash();

                        // loop through the outputs and push them into the database for the global indexes
                        for (const auto &output : arg.outputs)
                        {
                            auto error = put_transaction_output(db_tx, output);

                            if (error)
                            {
                                return error;
                            }
                        }
                    }
                    /**
                     * This transaction type is a snowflake in that it does not have an output subset as
                     * this type of transaction only ever contains a single output
                     */
                    else if constexpr (std::is_same_v<T, Types::Blockchain::stake_refund_transaction_t>)
                    {
                        const auto txn_hash = arg.hash();

                        /**
                         * We set the amount to 0 here as a) the amount is masked anyways and b) it does not
                         * matter for generating or checking signatures
                         */
                        const auto output =
                            Types::Blockchain::transaction_output_t(arg.public_ephemeral, 0, arg.commitment);

                        // push the output into the database
                        auto error = put_transaction_output(db_tx, output);

                        if (error)
                        {
                            return error;
                        }
                    }

                    return MAKE_ERROR(SUCCESS);
                },
                transaction);

            if (error)
            {
                return {error, txn_hash};
            }
        }

        return {MAKE_ERROR(SUCCESS), txn_hash};
    }

    Error BlockchainStorage::put_transaction_block_hash(
        std::unique_ptr<Database::LMDBTransaction> &db_tx,
        const crypto_hash_t &txn_hash,
        const crypto_hash_t &block_hash)
    {
        db_tx->set_database(m_transaction_block_hashes);

        return db_tx->put(txn_hash, block_hash);
    }

    Error BlockchainStorage::put_transaction_output(
        std::unique_ptr<Database::LMDBTransaction> &db_tx,
        const Types::Blockchain::transaction_output_t &output)
    {
        db_tx->set_database(m_transaction_outputs);

        return db_tx->put(output.hash(), output.serialize_output());
    }
} // namespace Core
