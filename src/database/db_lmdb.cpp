// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "db_lmdb.h"

#include <cppfs/FileHandle.h>
#include <cppfs/fs.h>
#include <hashing.h>

#define LMDB_SPACE_MULTIPLIER (1024 * 1024) // to MB

static ThreadSafeMap<crypto_hash_t, std::shared_ptr<Database::LMDB>> instances;

namespace Database
{
    LMDB::~LMDB()
    {
        close();
    }

    LMDB::operator MDB_env *&()
    {
        if (!m_env)
        {
            throw std::runtime_error("LMDB environment has been previously closed");
        }

        return m_env;
    }

    Error LMDB::close()
    {
        std::scoped_lock lock(m_mutex);

        {
            auto error = flush(true);

            if (error)
            {
                return error;
            }
        }

        mdb_env_close(m_env);

        m_env = nullptr;

        if (instances.contains(m_id))
        {
            instances.erase(m_id);
        }

        return MAKE_ERROR(SUCCESS);
    }

    Error LMDB::detect_map_size() const
    {
        std::scoped_lock lock(m_mutex);

        if (open_transactions() != 0)
        {
            return MAKE_ERROR_MSG(LMDB_ERROR, "Cannot detect LMDB environment map size while transactions are open");
        }

        const auto result = mdb_env_set_mapsize(m_env, 0);

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }

    Error LMDB::expand()
    {
        const auto [error, pages] = memory_to_pages(m_growth_factor * 1024 * 1024);

        if (error)
        {
            return error;
        }

        return expand(pages);
    }

    Error LMDB::expand(size_t pages)
    {
        std::scoped_lock lock(m_mutex);

        if (open_transactions() != 0)
        {
            return MAKE_ERROR_MSG(LMDB_ERROR, "Cannot expand LMDB environment map size while transactions are open");
        }

        const auto [info_error, l_info] = info();

        if (info_error)
        {
            return info_error;
        }

        const auto [stats_error, l_stats] = stats();

        if (stats_error)
        {
            return stats_error;
        }

        const auto new_size = (l_stats.ms_psize * pages) + l_info.me_mapsize;

        const auto result = mdb_env_set_mapsize(m_env, new_size);

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }

    Error LMDB::flush(bool force)
    {
        if (!m_env)
        {
            return MAKE_ERROR_MSG(LMDB_ERROR, "LMDB environment has been previously closed");
        }

        const auto result = mdb_env_sync(m_env, (force) ? 1 : 0);

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }

    std::shared_ptr<LMDBDatabase> LMDB::get_database(const crypto_hash_t &id)
    {
        if (m_databases.contains(id))
        {
            return m_databases.at(id);
        }

        throw std::invalid_argument("LMDB database not found");
    }

    std::tuple<Error, unsigned int> LMDB::get_flags() const
    {
        if (!m_env)
        {
            return {MAKE_ERROR_MSG(LMDB_ERROR, "LMDB environment has been previously closed."), 0};
        }

        unsigned int env_flags;

        const auto result = mdb_env_get_flags(m_env, &env_flags);

        return {MAKE_ERROR_MSG(result, MDB_STR_ERR(result)), env_flags};
    }

    std::shared_ptr<LMDB> LMDB::instance(const crypto_hash_t &id)
    {
        if (instances.contains(id))
        {
            return instances.at(id);
        }

        throw std::invalid_argument("LMDB environment not found");
    }

    size_t LMDB::growth_factor() const
    {
        return m_growth_factor;
    }

    crypto_hash_t LMDB::id() const
    {
        return m_id;
    }

    std::tuple<Error, MDB_envinfo> LMDB::info() const
    {
        MDB_envinfo info;

        if (!m_env)
        {
            return {MAKE_ERROR_MSG(LMDB_ERROR, "LMDB environment has been previously closed"), info};
        }

        const auto result = mdb_env_info(m_env, &info);

        return {MAKE_ERROR_MSG(result, MDB_STR_ERR(result)), info};
    }

    std::shared_ptr<LMDB>
        LMDB::instance(const std::string &path, int flags, int mode, size_t growth_factor, unsigned int max_databases)
    {
        const auto id = Crypto::Hashing::sha3(path.data(), path.size());

        if (!instances.contains(id))
        {
            auto db = std::make_shared<LMDB>();

            db->m_growth_factor = growth_factor;

            db->m_env = nullptr;

            db->m_open_txns = 0;

            db->m_id = id;

            auto file = cppfs::fs::open(path);

            if (flags & MDB_NOSUBDIR)
            {
                if (file.exists() && !file.isFile())
                {
                    throw std::runtime_error("LMDB path must be a regular file.");
                }
            }
            else
            {
                if (!file.isDirectory())
                {
                    file.createDirectory();
                }
            }

            auto success = mdb_env_create(&db->m_env);

            if (success != MDB_SUCCESS)
            {
                throw std::runtime_error("Could not create LMDB environment: " + MDB_STR_ERR(success));
            }

            success = mdb_env_set_mapsize(db->m_env, growth_factor * LMDB_SPACE_MULTIPLIER);

            if (success != MDB_SUCCESS)
            {
                throw std::runtime_error("Could not allocate initial LMDB memory map: " + MDB_STR_ERR(success));
            }

            success = mdb_env_set_maxdbs(db->m_env, max_databases);

            if (success != MDB_SUCCESS)
            {
                throw std::runtime_error(
                    "Could not set maximum number of LMDB named databases: " + MDB_STR_ERR(success));
            }

            /**
             * A transaction and its cursors must only be used by a single thread, and a thread may only have a single
             * write transaction at a time. If MDB_NOTLS is in use, this does not apply to read-only transactions.
             */
            success = mdb_env_open(db->m_env, path.c_str(), flags | MDB_NOTLS, mode);

            if (success != MDB_SUCCESS)
            {
                mdb_env_close(db->m_env);

                throw std::runtime_error("Could not open LMDB database file: " + path + ": " + MDB_STR_ERR(success));
            }

            instances.insert(id, db);
        }

        return instances.at(id);
    }

    std::tuple<Error, size_t> LMDB::memory_to_pages(size_t memory) const
    {
        const auto [error, l_stats] = stats();

        return {MAKE_ERROR(SUCCESS), size_t(ceil(double(memory) / double(l_stats.ms_psize)))};
    }

    std::tuple<Error, size_t> LMDB::max_key_size() const
    {
        if (!m_env)
        {
            return {MAKE_ERROR_MSG(LMDB_ERROR, "LMDB environment has been previously closed"), 0};
        }

        const auto result = mdb_env_get_maxkeysize(m_env);

        return {MAKE_ERROR(SUCCESS), result};
    }

    std::tuple<Error, unsigned int> LMDB::max_readers() const
    {
        if (!m_env)
        {
            return {MAKE_ERROR_MSG(LMDB_ERROR, "LMDB environment has been previously closed"), 0};
        }

        unsigned int readers = 0;

        const auto result = mdb_env_get_maxreaders(m_env, &readers);

        return {MAKE_ERROR_MSG(result, MDB_STR_ERR(result)), readers};
    }

    std::shared_ptr<LMDBDatabase> LMDB::open_database(const std::string &name, int flags)
    {
        auto id = Crypto::Hashing::sha3(name.data(), name.size());

        if (!m_databases.contains(id))
        {
            auto env = LMDB::instance(m_id);

            auto db = std::make_shared<LMDBDatabase>(env, name, flags);

            m_databases.insert(id, db);
        }

        return m_databases.at(id);
    }

    size_t LMDB::open_transactions() const
    {
        std::scoped_lock lock(m_txn_mutex);

        return m_open_txns;
    }

    Error LMDB::set_flags(int flags, bool flag_state)
    {
        std::scoped_lock lock(m_mutex);

        const auto result = mdb_env_set_flags(m_env, flags, (flag_state) ? 1 : 0);

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }

    std::tuple<Error, MDB_stat> LMDB::stats() const
    {
        MDB_stat stats;

        if (!m_env)
        {
            return {MAKE_ERROR_MSG(LMDB_ERROR, "LMDB enviroment has been previously closed"), stats};
        }

        const auto result = mdb_env_stat(m_env, &stats);

        return {MAKE_ERROR_MSG(result, MDB_STR_ERR(result)), stats};
    }

    std::unique_ptr<LMDBTransaction> LMDB::transaction(bool readonly) const
    {
        auto instance = LMDB::instance(id());

        return std::make_unique<LMDBTransaction>(instance, readonly);
    }

    void LMDB::transaction_register(const LMDBTransaction &txn)
    {
        if (txn.readonly())
        {
            return;
        }

        std::scoped_lock lock(m_txn_mutex);

        m_open_txns++;
    }

    void LMDB::transaction_unregister(const LMDBTransaction &txn)
    {
        if (txn.readonly())
        {
            return;
        }

        std::scoped_lock lock(m_txn_mutex);

        if (m_open_txns > 0)
        {
            m_open_txns--;
        }
    }

    std::tuple<int, int, int> LMDB::version()
    {
        int major, minor, patch;

        mdb_version(&major, &minor, &patch);

        return {major, minor, patch};
    }

    LMDBDatabase::LMDBDatabase(std::shared_ptr<LMDB> &env, const std::string &name, int flags): m_env(env), m_dbi(0)
    {
        m_id = Crypto::Hashing::sha3(name.data(), name.size()).to_string();

        const auto [error, env_flags] = env->get_flags();

        const auto readonly = (env_flags & MDB_RDONLY);

        auto txn = std::make_unique<LMDBTransaction>(m_env, readonly);

        auto success = mdb_dbi_open(*txn, name.empty() ? nullptr : name.c_str(), flags | MDB_CREATE, &m_dbi);

        if (success != MDB_SUCCESS)
        {
            throw std::runtime_error("Unable to open LMDB named database: " + MDB_STR_ERR(success));
        }

        if (!(env_flags & MDB_RDONLY))
        {
            if (txn->commit() != SUCCESS)
            {
                throw std::runtime_error("Could not commit to open LMDB named database: " + MDB_STR_ERR(success));
            }
        }
    }

    LMDBDatabase::~LMDBDatabase()
    {
        mdb_dbi_close(*m_env, m_dbi);
    }

    LMDBDatabase::operator MDB_dbi &()
    {
        return m_dbi;
    }

    size_t LMDBDatabase::count()
    {
        auto db = m_env->get_database(m_id);

        auto txn = db->transaction(true);

        auto cursor = txn->cursor();

        MDB_val key, value;

        size_t count = 0;

        while (mdb_cursor_get(*cursor, &key, &value, count ? MDB_NEXT : MDB_FIRST) == MDB_SUCCESS)
        {
            count++;
        }

        return count;
    }

    Error LMDBDatabase::del(const serializer_t &key)
    {
    try_again:
        auto txn = transaction();

        auto error = txn->del(key);

        MDB_CHECK_TXN_EXPAND(error, m_env, txn, try_again);

        if (error)
        {
            return error;
        }

        error = txn->commit();

        MDB_CHECK_TXN_EXPAND(error, m_env, txn, try_again);

        return error;
    }

    Error LMDBDatabase::del(const uint64_t &key)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        return del(i_key);
    }

    Error LMDBDatabase::del(const serializer_t &key, const serializer_t &value)
    {
    try_again:
        auto txn = transaction();

        auto error = txn->del(key, value);

        MDB_CHECK_TXN_EXPAND(error, m_env, txn, try_again);

        if (error)
        {
            return error;
        }

        error = txn->commit();

        MDB_CHECK_TXN_EXPAND(error, m_env, txn, try_again);

        return error;
    }

    Error LMDBDatabase::del(const uint64_t &key, const serializer_t &value)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        return del(i_key, value);
    }

    Error LMDBDatabase::drop(bool delete_db)
    {
        std::scoped_lock lock(m_db_mutex);

    try_again:

        auto txn = std::make_unique<LMDBTransaction>(m_env);

        auto result = mdb_drop(*txn, m_dbi, (delete_db) ? 1 : 0);

        if (result == MDB_MAP_FULL)
        {
            txn->abort();

            m_env->expand();

            goto try_again;
        }

        return txn->commit();
    }

    std::shared_ptr<LMDB> LMDBDatabase::env() const
    {
        return m_env;
    }

    bool LMDBDatabase::exists(const serializer_t &key)
    {
        auto txn = transaction(true);

        return txn->exists(key);
    }

    bool LMDBDatabase::exists(const uint64_t &key)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        return exists(i_key);
    }

    std::tuple<Error, deserializer_t> LMDBDatabase::get(const serializer_t &key)
    {
        auto txn = transaction(true);

        return txn->get(key);
    }

    std::tuple<Error, deserializer_t> LMDBDatabase::get(const uint64_t &key)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        return get(i_key);
    }

    std::vector<deserializer_t> LMDBDatabase::get_all()
    {
        std::vector<deserializer_t> results;

        const auto keys = list_keys();

        for (auto &key : keys)
        {
            serializer_t i_key(key.unread_data());

            auto [error, value] = get(i_key);

            if (!error)
            {
                results.push_back(value);
            }
        }

        return results;
    }

    std::tuple<Error, unsigned int> LMDBDatabase::get_flags()
    {
        auto txn = transaction(true);

        unsigned int dbi_flags;

        const auto result = mdb_dbi_flags(*txn, m_dbi, &dbi_flags);

        return {MAKE_ERROR_MSG(result, MDB_STR_ERR(result)), dbi_flags};
    }

    crypto_hash_t LMDBDatabase::id() const
    {
        return m_id;
    }

    std::vector<deserializer_t> LMDBDatabase::list_keys(bool ignore_duplicates)
    {
        auto txn = transaction(true);

        auto cursor = txn->cursor();

        std::vector<deserializer_t> results;

        MDB_val key, value;

        size_t count = 0;

        deserializer_t last_key({});

        while (mdb_cursor_get(*cursor, &key, &value, count ? MDB_NEXT : MDB_FIRST) == MDB_SUCCESS)
        {
            auto data = FROM_MDB_VAL(key);

            deserializer_t r_key(data);

            if (ignore_duplicates && r_key.to_string() == last_key.to_string())
            {
                continue;
            }

            results.push_back(r_key);

            last_key = r_key;

            count++;
        }

        return results;
    }

    Error LMDBDatabase::put(const serializer_t &key, const serializer_t &value, int flags)
    {
    try_again:
        auto txn = transaction();

        {
            auto error = txn->put(key, value, flags);

            MDB_CHECK_TXN_EXPAND(error, m_env, txn, try_again);

            if (error)
            {
                return error;
            }
        }

        auto error = txn->commit();

        MDB_CHECK_TXN_EXPAND(error, m_env, txn, try_again);

        return error;
    }

    Error LMDBDatabase::put(const uint64_t &key, const serializer_t &value, int flags)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        return put(i_key, value, flags);
    }

    std::unique_ptr<LMDBTransaction> LMDBDatabase::transaction(bool readonly)
    {
        if (m_dbi == 0)
        {
            throw std::runtime_error("LMDB database no longer exists");
        }

        std::scoped_lock lock(m_db_mutex);

        auto db = m_env->get_database(m_id);

        return std::make_unique<LMDBTransaction>(m_env, db, readonly);
    }

    LMDBTransaction::LMDBTransaction(std::shared_ptr<LMDB> &env, bool readonly): m_env(env), m_readonly(readonly)
    {
        txn_setup();
    }

    LMDBTransaction::LMDBTransaction(std::shared_ptr<LMDB> &env, std::shared_ptr<LMDBDatabase> &db, bool readonly):
        m_env(env), m_db(db), m_readonly(readonly)
    {
        txn_setup();
    }

    LMDBTransaction::~LMDBTransaction()
    {
        // default action is to abort if the Transaction leaves scope
        abort();
    }

    LMDBTransaction::operator MDB_txn *&()
    {
        return *m_txn;
    }

    void LMDBTransaction::abort()
    {
        if (!m_txn)
        {
            return;
        }

        mdb_txn_abort(*m_txn);

        m_env->transaction_unregister(*this);

        m_txn = nullptr;
    }

    Error LMDBTransaction::commit()
    {
        if (!m_txn)
        {
            return MAKE_ERROR_MSG(LMDB_ERROR, MDB_STR_ERR(MDB_BAD_TXN));
        }

        const auto result = mdb_txn_commit(*m_txn);

        m_env->transaction_unregister(*this);

        m_txn = nullptr;

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }

    std::unique_ptr<LMDBCursor> LMDBTransaction::cursor()
    {
        return std::make_unique<LMDBCursor>(m_txn, m_db, m_readonly);
    }

    std::shared_ptr<LMDB> LMDBTransaction::env() const
    {
        return m_env;
    }

    Error LMDBTransaction::del(const serializer_t &key)
    {
        MDB_VAL(key, i_key);

        const auto result = mdb_del(*m_txn, *m_db, &i_key, nullptr);

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }

    Error LMDBTransaction::del(const serializer_t &key, const serializer_t &value)
    {
        MDB_VAL(key, i_key);

        MDB_VAL(value, i_value);

        const auto result = mdb_del(*m_txn, *m_db, &i_key, &i_value);

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }

    Error LMDBTransaction::del(const uint64_t &key)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        return del(i_key);
    }

    Error LMDBTransaction::del(const uint64_t &key, const serializer_t &value)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        return del(i_key, value);
    }

    bool LMDBTransaction::exists(const serializer_t &key)
    {
        MDB_VAL(key, i_key);

        MDB_val value;

        const auto result = mdb_get(*m_txn, *m_db, &i_key, &value);

        return result == MDB_SUCCESS;
    }

    bool LMDBTransaction::exists(const uint64_t &key)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        return exists(i_key);
    }

    std::tuple<Error, deserializer_t> LMDBTransaction::get(const serializer_t &key)
    {
        MDB_VAL(key, i_key);

        MDB_val value;

        const auto result = mdb_get(*m_txn, *m_db, &i_key, &value);

        deserializer_t reader({});

        if (result == MDB_SUCCESS)
        {
            std::vector<uint8_t> data = FROM_MDB_VAL(value);

            reader = deserializer_t(data);
        }

        return {MAKE_ERROR_MSG(result, MDB_STR_ERR(result)), reader};
    }

    std::tuple<Error, deserializer_t> LMDBTransaction::get(const uint64_t &key)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        return get(i_key);
    }

    std::tuple<Error, size_t> LMDBTransaction::id() const
    {
        if (!m_txn)
        {
            return {MAKE_ERROR_MSG(LMDB_BAD_TXN, "Transaction does not exist"), 0};
        }

        const auto result = mdb_txn_id(*m_txn);

        return {MAKE_ERROR(SUCCESS), result};
    }

    Error LMDBTransaction::put(const serializer_t &key, const serializer_t &value, int flags)
    {
        MDB_VAL(key, i_key);

        MDB_VAL(value, i_value);

        const auto result = mdb_put(*m_txn, *m_db, &i_key, &i_value, flags);

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }

    Error LMDBTransaction::put(const uint64_t &key, const serializer_t &value, int flags)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        return put(i_key, value, flags);
    }

    bool LMDBTransaction::readonly() const
    {
        return m_readonly;
    }

    Error LMDBTransaction::renew()
    {
        if (!m_txn || !m_readonly)
        {
            return MAKE_ERROR_MSG(LMDB_BAD_TXN, "Transaction does not exist");
        }

        const auto result = mdb_txn_renew(*m_txn);

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }

    Error LMDBTransaction::reset()
    {
        if (!m_txn || !m_readonly)
        {
            return MAKE_ERROR_MSG(LMDB_BAD_TXN, "Transaction does not exist or is readonly");
        }

        const auto result = mdb_txn_renew(*m_txn);

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }

    void LMDBTransaction::set_database(std::shared_ptr<LMDBDatabase> &db)
    {
        m_db = db;
    }

    void LMDBTransaction::txn_setup()
    {
        MDB_txn *result;

        for (int i = 0; i < 3; ++i)
        {
            const auto mdb_result = mdb_txn_begin(*m_env, nullptr, (m_readonly) ? MDB_RDONLY : 0, &result);

            if (mdb_result == MDB_SUCCESS)
            {
                break;
            }

            if (mdb_result == MDB_MAP_RESIZED && i < 2)
            {
                m_env->detect_map_size();

                continue;
            }

            throw std::runtime_error("Unable to start LMDB transaction: " + MDB_STR_ERR(mdb_result));
        }

        m_txn = std::make_shared<MDB_txn *>(result);

        m_env->transaction_register(*this);
    }

    LMDBCursor::LMDBCursor(std::shared_ptr<MDB_txn *> &txn, std::shared_ptr<LMDBDatabase> &db, bool readonly):
        m_txn(txn), m_db(db), m_readonly(readonly)
    {
        const auto result = mdb_cursor_open(*m_txn, *m_db, &m_cursor);

        if (result != MDB_SUCCESS)
        {
            throw std::runtime_error("Could not open LMDB cursor: " + MDB_STR_ERR(result));
        }
    }

    LMDBCursor::~LMDBCursor()
    {
        if (m_cursor == nullptr || !m_readonly)
        {
            return;
        }

        mdb_cursor_close(m_cursor);

        m_cursor = nullptr;
    }

    LMDBCursor::operator MDB_cursor *&()
    {
        return m_cursor;
    }

    std::tuple<Error, size_t> LMDBCursor::count()
    {
        if (m_cursor == nullptr)
        {
            return {MAKE_ERROR_MSG(LMDB_ERROR, "Cursor does not exist"), 0};
        }

        size_t count = 0;

        const auto result = mdb_cursor_count(m_cursor, &count);

        return {MAKE_ERROR_MSG(result, MDB_STR_ERR(result)), count};
    }

    Error LMDBCursor::del(int flags)
    {
        if (m_cursor == nullptr)
        {
            return MAKE_ERROR_MSG(LMDB_ERROR, "Cursor does not exist");
        }

        const auto result = mdb_cursor_del(m_cursor, flags);

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }

    std::tuple<Error, deserializer_t, deserializer_t> LMDBCursor::get(const MDB_cursor_op &op)
    {
        if (m_cursor == nullptr)
        {
            return {MAKE_ERROR_MSG(LMDB_ERROR, "Cursor does not exist"), {}, {}};
        }

        MDB_val i_key, i_value;

        const auto result = mdb_cursor_get(m_cursor, &i_key, &i_value, op);

        deserializer_t key({}), value({});

        if (result == MDB_SUCCESS)
        {
            std::vector<uint8_t> data = FROM_MDB_VAL(i_key);

            key = deserializer_t(data);

            data = FROM_MDB_VAL(i_value);

            value = deserializer_t(data);
        }

        return {MAKE_ERROR_MSG(result, MDB_STR_ERR(result)), key, value};
    }

    std::tuple<Error, deserializer_t, deserializer_t> LMDBCursor::get(const serializer_t &key, const MDB_cursor_op &op)
    {
        if (m_cursor == nullptr)
        {
            return {MAKE_ERROR_MSG(LMDB_ERROR, "Cursor does not exist"), {}, {}};
        }

        MDB_val i_value;

        MDB_VAL(key, i_key);

        const auto result = mdb_cursor_get(m_cursor, &i_key, &i_value, op);

        deserializer_t r_key({}), value({});

        if (result == MDB_SUCCESS)
        {
            std::vector<uint8_t> data = FROM_MDB_VAL(i_key);

            r_key = deserializer_t(data);

            data = FROM_MDB_VAL(i_value);

            value = deserializer_t(data);
        }

        return {MAKE_ERROR_MSG(result, MDB_STR_ERR(result)), r_key, value};
    }

    std::tuple<Error, uint64_t, deserializer_t> LMDBCursor::get(const uint64_t &key, const MDB_cursor_op &op)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        auto [error, r_key, r_value] = get(i_key, op);

        if (error)
        {
            return {error, {}, {}};
        }

        return {error, r_key.uint64(false, true), r_value};
    }

    std::tuple<Error, deserializer_t, std::vector<deserializer_t>> LMDBCursor::get_all(const serializer_t &key)
    {
        std::vector<deserializer_t> results;

        bool success = false;

        do
        {
            auto [error, k, v] = get(key, (!success) ? MDB_SET : MDB_NEXT_DUP);

            if (!error)
            {
                results.push_back(v);
            }

            success = error == SUCCESS;
        } while (success);

        Error error = (!results.empty()) ? SUCCESS : LMDB_EMPTY;

        return {error, deserializer_t(key), results};
    }

    std::tuple<Error, uint64_t, std::vector<deserializer_t>> LMDBCursor::get_all(const uint64_t &key)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        auto [error, r_key, r_value] = get_all(i_key);

        if (error)
        {
            return {error, {}, {}};
        }

        return {error, r_key.uint64(false, true), r_value};
    }

    Error LMDBCursor::put(const serializer_t &key, const serializer_t &value, int flags)
    {
        MDB_VAL(key, i_key);

        MDB_VAL(value, i_value);

        const auto result = mdb_cursor_put(m_cursor, &i_key, &i_value, flags);

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }

    Error LMDBCursor::put(const uint64_t &key, const serializer_t &value, int flags)
    {
        serializer_t i_key;

        i_key.uint64(key, true);

        return put(i_key, value, flags);
    }

    Error LMDBCursor::renew()
    {
        if (m_cursor == nullptr || !m_readonly)
        {
            return MAKE_ERROR_MSG(LMDB_ERROR, "Cursor does not exist or is read only.");
        }

        const auto result = mdb_cursor_renew(*m_txn, m_cursor);

        return MAKE_ERROR_MSG(result, MDB_STR_ERR(result));
    }
} // namespace Database
