// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "blockchain_storage.h"

static ThreadSafeMap<crypto_hash_t, std::shared_ptr<Core::BlockchainStorage>> instances;

namespace Core
{
    BlockchainStorage::BlockchainStorage(const std::string &db_path)
    {
        m_id = Crypto::Hashing::sha3(db_path.data(), db_path.size());

        m_db_env = Database::LMDB::instance(db_path, 0, 0600, 16, 8);

        m_blocks = m_db_env->open_database("blocks");

        m_block_indexes = m_db_env->open_database("block_indexes");

        m_block_timestamps = m_db_env->open_database("block_timestamps");

        m_transactions = m_db_env->open_database("transactions");

        m_key_images = m_db_env->open_database("key_images");

        m_transaction_outputs = m_db_env->open_database("transaction_outputs");
    }

    BlockchainStorage::~BlockchainStorage()
    {
        if (instances.contains(m_id))
        {
            instances.erase(m_id);
        }
    }

    bool BlockchainStorage::block_exists(const crypto_hash_t &block_hash) const
    {
        return m_blocks->exists(block_hash);
    }

    bool BlockchainStorage::block_exists(const uint64_t &block_index) const
    {
        return m_block_indexes->exists(block_index);
    }

    Error BlockchainStorage::del_block(const uint64_t &block_index)
    {
        auto [block_error, block, transactions] = get_block(block_index);

        if (block_error)
        {
            return block_error;
        }

    try_again:
        auto txn = m_blocks->transaction();

        for (const auto &transaction : transactions)
        {
            auto error = del_transaction(txn, transaction);

            MDB_CHECK_TXN_EXPAND(error, m_db_env, txn, try_again);

            if (error)
            {
                return error;
            }
        }

        // delete block timestamp
        {
            txn->set_database(m_block_timestamps);

            auto error = txn->del(block.timestamp, block.hash());

            MDB_CHECK_TXN_EXPAND(error, m_db_env, txn, try_again);

            if (error)
            {
                return error;
            }
        }

        // delete block index
        {
            txn->set_database(m_block_indexes);

            auto error = txn->del(block.block_index);

            MDB_CHECK_TXN_EXPAND(error, m_db_env, txn, try_again);

            if (error)
            {
                return error;
            }
        }

        // delete block
        {
            txn->set_database(m_blocks);

            auto error = txn->del(block.hash());

            MDB_CHECK_TXN_EXPAND(error, m_db_env, txn, try_again);

            if (error)
            {
                return error;
            }
        }

        auto error = txn->commit();

        MDB_CHECK_TXN_EXPAND(error, m_db_env, txn, try_again);

        return error;
    }

    Error BlockchainStorage::del_key_image(
        std::unique_ptr<Database::LMDBTransaction> &db_tx,
        const crypto_key_image_t &key_image)
    {
        db_tx->set_database(m_key_images);

        return db_tx->del(key_image);
    }

    Error BlockchainStorage::del_transaction(
        std::unique_ptr<Database::LMDBTransaction> &db_tx,
        const transaction_t &transaction)
    {
        return std::visit(
            [&](auto &&tx)
            {
                USEVARIANT(T, tx);

                if COMMITED_USER_TX_VARIANT (T)
                {
                    // delete the key images
                    for (const auto &key_image : tx.key_images)
                    {
                        auto error = del_key_image(db_tx, key_image);

                        if (error)
                        {
                            return error;
                        }
                    }

                    // delete the outputs
                    for (const auto &output : tx.outputs)
                    {
                        auto error = del_transaction_output(db_tx, output.hash());

                        if (error)
                        {
                            return error;
                        }
                    }
                }
                else if VARIANT (T, genesis_transaction_t)
                {
                    // delete the outputs
                    for (const auto &output : tx.outputs)
                    {
                        auto error = del_transaction_output(db_tx, output.hash());

                        if (error)
                        {
                            return error;
                        }
                    }
                }
                else if VARIANT (T, stake_refund_transaction_t)
                {
                    // delete the outputs
                    for (const auto &output : tx.outputs)
                    {
                        auto error = del_transaction_output(db_tx, output.hash());

                        if (error)
                        {
                            return error;
                        }
                    }
                }

                db_tx->set_database(m_transactions);

                return db_tx->del(tx.hash());
            },
            transaction);
    }

    Error BlockchainStorage::del_transaction_output(
        std::unique_ptr<Database::LMDBTransaction> &db_tx,
        const crypto_hash_t &output_hash)
    {
        db_tx->set_database(m_transaction_outputs);

        return db_tx->del(output_hash);
    }

    std::tuple<Error, block_t, std::vector<transaction_t>>
        BlockchainStorage::get_block(const crypto_hash_t &block_hash) const
    {
        // go get the block
        const auto [error, block] = m_blocks->get<block_t>(block_hash);

        if (error)
        {
            return {MAKE_ERROR(DB_BLOCK_NOT_FOUND), {}, {}};
        }

        std::vector<transaction_t> transactions;

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

    std::tuple<Error, block_t, std::vector<transaction_t>>
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
        const auto [error, block] = m_blocks->get<block_t>(block_hash);

        if (error)
        {
            return {MAKE_ERROR(DB_BLOCK_NOT_FOUND), 0};
        }

        return {error, block.block_index};
    }

    std::tuple<Error, std::vector<transaction_output_t>>
        BlockchainStorage::get_random_outputs(uint64_t block_index, size_t count) const
    {
        std::vector<transaction_output_t> results;

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

            auto [error, key, value] = cursor->get(random_hash, MDB_SET_RANGE);

            if (error == LMDB_NOTFOUND)
            {
                continue;
            }

            transaction_output_t output;

            output.deserialize(value);

            const auto unlock_block = value.varint<uint64_t>();

            if (output.hash() != key || unlock_block < block_index)
            {
                continue;
            }

            if (std::find(results.begin(), results.end(), output) == results.end())
            {
                results.push_back(output);
            }
        }

        std::sort(results.begin(), results.end());

        return {MAKE_ERROR(SUCCESS), results};
    }

    std::tuple<Error, transaction_t, crypto_hash_t>
        BlockchainStorage::get_transaction(const crypto_hash_t &txn_hash) const
    {
        // go get the transaction
        const auto [error, txn_data] = m_transactions->get(txn_hash);

        if (error)
        {
            return {MAKE_ERROR(DB_TRANSACTION_NOT_FOUND), {}, {}};
        }

        deserializer_t reader(txn_data);

        const auto block_hash = reader.key<crypto_hash_t>();

        // figure out what type of transaction it is
        const auto type = reader.varint<uint64_t>(true);

        // depending on the transaction type, we'll return the proper structure
        switch (type)
        {
            case TransactionType::GENESIS:
                return {MAKE_ERROR(SUCCESS), genesis_transaction_t(reader), block_hash};
            case TransactionType::STAKER:
                return {MAKE_ERROR(SUCCESS), staker_transaction_t(reader), block_hash};
            case TransactionType::NORMAL:
                return {MAKE_ERROR(SUCCESS), committed_normal_transaction_t(reader), block_hash};
            case TransactionType::STAKE:
                return {MAKE_ERROR(SUCCESS), committed_stake_transaction_t(reader), block_hash};
            case TransactionType::RECALL_STAKE:
                return {MAKE_ERROR(SUCCESS), committed_recall_stake_transaction_t(reader), block_hash};
            case TransactionType::STAKE_REFUND:
                return {MAKE_ERROR(SUCCESS), stake_refund_transaction_t(reader), block_hash};
            default:
                return {MAKE_ERROR(UNKNOWN_TRANSACTION_TYPE), {}, block_hash};
        }
    }

    std::tuple<Error, transaction_output_t, uint64_t>
        BlockchainStorage::get_transaction_output(const crypto_hash_t &output_hash) const
    {
        auto [error, output_data] = m_transaction_outputs->get(output_hash);

        if (error)
        {
            return {MAKE_ERROR(DB_TRANSACTION_OUTPUT_NOT_FOUND), {}, {}};
        }

        transaction_output_t output;

        output.deserialize(output_data);

        const auto unlock_block = output_data.varint<uint64_t>();

        return {MAKE_ERROR(SUCCESS), output, unlock_block};
    }

    std::tuple<Error, std::vector<std::tuple<transaction_output_t, uint64_t>>>
        BlockchainStorage::get_transaction_output(const std::vector<crypto_hash_t> &output_hashes) const
    {
        std::vector<std::tuple<transaction_output_t, uint64_t>> results;

        for (const auto &output_hash : output_hashes)
        {
            const auto [error, output, unlock_block] = get_transaction_output(output_hash);

            if (error)
            {
                return {error, {}};
            }

            results.emplace_back(output, unlock_block);
        }

        return {MAKE_ERROR(SUCCESS), results};
    }

    std::shared_ptr<BlockchainStorage> BlockchainStorage::instance(const std::string &db_path)
    {
        const auto id = Crypto::Hashing::sha3(db_path.data(), db_path.size());

        if (!instances.contains(id))
        {
            auto db = new BlockchainStorage(db_path);

            auto ptr = std::shared_ptr<BlockchainStorage>(db);

            instances.insert(id, ptr);
        }

        return instances.at(id);
    }

    bool BlockchainStorage::key_image_exists(const crypto_key_image_t &key_image) const
    {
        return m_key_images->exists(key_image);
    }

    bool BlockchainStorage::key_image_exists(const std::vector<crypto_key_image_t> &key_images) const
    {
        std::vector<crypto_key_image_t> results;

        auto txn = m_key_images->transaction(true);

        bool exists = false;

        // loop through the requested key images
        for (const auto &key_image : key_images)
        {
            // check to see if the key image exists
            if (txn->exists(key_image))
            {
                exists = true;
            }
        }

        return exists;
    }

    size_t BlockchainStorage::output_count() const
    {
        return m_transaction_outputs->count();
    }

    bool BlockchainStorage::output_exists(const crypto_hash_t &output_hash) const
    {
        return m_transaction_outputs->exists(output_hash);
    }

    Error BlockchainStorage::put_block(const block_t &block, const std::vector<transaction_t> &transactions)
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
                std::visit([&](auto &&arg) { return put_transaction(db_tx, arg, block_hash); }, block.reward_tx);

            MDB_CHECK_TXN_EXPAND(error, m_db_env, db_tx, try_again);

            if (error)
            {
                return error;
            }
        }

        // loop through the individual transactions in the block and push them into the database
        for (const auto &transaction : transactions)
        {
            auto [error, txn_hash] = put_transaction(db_tx, transaction, block_hash);

            MDB_CHECK_TXN_EXPAND(error, m_db_env, db_tx, try_again);

            if (error)
            {
                return error;
            }
        }

        // push the block itself into the database
        {
            db_tx->set_database(m_blocks);

            auto error = db_tx->put(block_hash, block);

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

        return db_tx->put(key_image);
    }

    std::tuple<Error, crypto_hash_t> BlockchainStorage::put_transaction(
        std::unique_ptr<Database::LMDBTransaction> &db_tx,
        const transaction_t &transaction,
        const crypto_hash_t &block_hash)
    {
        db_tx->set_database(m_transactions);

        crypto_hash_t txn_hash;

        /**
         * The reason that this looks so convoluted is because of the use
         * of std::variant for the different types of transactions
         */
        {
            auto error = std::visit(
                [&](auto &&arg)
                {
                    USEVARIANT(T, arg);

                    {
                        txn_hash = arg.hash();

                        serializer_t writer;

                        arg.serialize(writer);

                        writer.key(block_hash);

                        // push the transaction itself into the database
                        auto error = db_tx->put(txn_hash, writer);

                        if (error)
                        {
                            return error;
                        }
                    }

                    // handle the key images on each type of transaction
                    if COMMITED_USER_TX_VARIANT (T)
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
                [&](auto &&arg)
                {
                    USEVARIANT(T, arg);

                    // handle the different types of transactions
                    if COMMITED_USER_TX_VARIANT (T)
                    {
                        const auto txn_hash = arg.hash();

                        // loop through the outputs and push them into the database for the global indexes
                        for (const auto &output : arg.outputs)
                        {
                            auto error = put_transaction_output(db_tx, output, arg.unlock_block);

                            if (error)
                            {
                                return error;
                            }
                        }
                    }
                    else if VARIANT (T, genesis_transaction_t)
                    {
                        const auto txn_hash = arg.hash();

                        // loop through the outputs and push them into the database for the global indexes
                        for (const auto &output : arg.outputs)
                        {
                            auto error = put_transaction_output(db_tx, output, arg.unlock_block);

                            if (error)
                            {
                                return error;
                            }
                        }
                    }
                    else if VARIANT (T, stake_refund_transaction_t)
                    {
                        const auto txn_hash = arg.hash();

                        // loop through the outputs and put them in the database
                        for (const auto &output : arg.outputs)
                        {
                            // push the output into the database
                            auto error = put_transaction_output(db_tx, output, arg.unlock_block);

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

        return {MAKE_ERROR(SUCCESS), txn_hash};
    }

    Error BlockchainStorage::put_transaction_output(
        std::unique_ptr<Database::LMDBTransaction> &db_tx,
        const transaction_output_t &output,
        uint64_t unlock_block)
    {
        db_tx->set_database(m_transaction_outputs);

        // we pack the unlock block on to the output for storage
        serializer_t writer;

        writer.varint(unlock_block);

        output.serialize(writer);

        return db_tx->put(output.hash(), writer);
    }

    Error BlockchainStorage::rewind(const uint64_t &block_index)
    {
        if (!block_exists(block_index))
        {
            return MAKE_ERROR(DB_BLOCK_NOT_FOUND);
        }

        const auto block_count = get_block_count();

        for (uint64_t i = block_count; i > block_index; --i)
        {
            auto error = del_block(i);

            if (error)
            {
                return error;
            }
        }

        return MAKE_ERROR(SUCCESS);
    }

    bool BlockchainStorage::transaction_exists(const crypto_hash_t &txn_hash) const
    {
        return m_transactions->exists(txn_hash);
    }
} // namespace Core
