// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef DATABASE_LMDB_H
#define DATABASE_LMDB_H

#include <crypto_types.h>
#include <errors.h>
#include <lmdb.h>
#include <map>
#include <memory>
#include <mutex>
#include <serializer.h>
#include <tuple>

#define MDB_STR_ERR(variable) std::string(mdb_strerror(variable))
#define MDB_VAL(input, output) MDB_val output = {(input).size(), (void *)(input).data()}
#define MDB_VAL_NUM(input, output) MDB_val output = {sizeof(input), (void *)&(input)}
#define FROM_MDB_VAL(value)                                  \
    std::vector<uint8_t>(                                    \
        static_cast<const unsigned char *>((value).mv_data), \
        static_cast<const unsigned char *>((value).mv_data) + (value).mv_size)
#define MDB_CHECK_TXN_EXPAND(error, env, txn, label)      \
    if (error == LMDB_MAP_FULL || error == LMDB_TXN_FULL) \
    {                                                     \
        txn->abort();                                     \
                                                          \
        const auto exp_error = env->expand();             \
                                                          \
        if (!exp_error)                                   \
        {                                                 \
            goto label;                                   \
        }                                                 \
    }

namespace Database
{
    // forward declarations
    class LMDBDatabase;
    class LMDBTransaction;
    class LMDBCursor;

    /**
     * Wraps the LMDB C API into an OOP model that allows for opening and using
     * multiple environments and databases at once.
     */
    class LMDB
    {
      public:
        ~LMDB();

        operator MDB_env *&();

        /**
         * Closes the environment
         */
        Error close();

        /**
         * Detects the current memory map size if it has been changed elsewhere
         * This requires that there are no open R/W transactions; otherwise, the method
         * will throw an exception.
         */
        Error detect_map_size() const;

        /**
         * Expands the memory map by the growth factor supplied to the constructor
         * This requires that there are no open R/W transactions; otherwise, the method
         * will throw an exception.
         */
        Error expand();

        /**
         * Expands the memory map by the number of pages specified.
         * This requires that there are no open R/W transactions; otherwise, the method
         * will throw an exception.
         *
         * @param pages
         */
        Error expand(size_t pages);

        /**
         * Flush the data buffers to disk.
         * Data is always written to disk when a transaction is committed, but the operating system may keep it
         * buffered. LMDB always flushes the OS buffers upon commit as well, unless the environment was opened
         * with MDB_NOSYNC or in part MDB_NOMETASYNC.
         *
         * This call is not valid if the environment was opened with MDB_RDONLY.
         *
         * @param force a synchronous flush of the buffers to disk
         */
        Error flush(bool force = false);

        /**
         * Retrieves an already open database by its ID
         *
         * @param id
         * @return
         */
        std::shared_ptr<LMDBDatabase> get_database(const std::string &id);

        /**
         * Retrieves the LMDB environment flags
         *
         * @return
         */
        std::tuple<Error, unsigned int> get_flags() const;

        /**
         * Retrieves an existing instance of an environment by its ID
         *
         * @param id
         * @return
         */
        static std::shared_ptr<LMDB> get_instance(const std::string &id);

        /**
         * Opens a LMDB environment using the specified parameters
         *
         * @param path
         * @param flags
         * @param mode
         * @param growth_factor in MB
         * @param max_databases
         * @return
         */
        static std::shared_ptr<LMDB> getInstance(
            const std::string &path,
            int flags = MDB_NOSUBDIR,
            int mode = 0600,
            size_t growth_factor = 8,
            unsigned int max_databases = 8);

        /**
         * Retrieves the current environment growth factor (in MB)
         *
         * @return
         */
        size_t growth_factor() const;

        /**
         * Retrieves the environments ID
         *
         * @return
         */
        std::string id() const;

        /**
         * Retrieves the LMDB environment information
         *
         * @return
         */
        std::tuple<Error, MDB_envinfo> info() const;

        /**
         * Retrieves the maximum byte size of a key in the LMDB environment
         *
         * @return
         */
        std::tuple<Error, size_t> max_key_size() const;

        /**
         * Retrieves the maximum number of readers for the LMDB environment
         *
         * @return
         */
        std::tuple<Error, unsigned int> max_readers() const;

        /**
         * Opens a database (separate key space) in the environment as a logical
         * partitioning of data.
         *
         * @param name
         * @param flags
         * @return
         */
        std::shared_ptr<LMDBDatabase> open_database(const std::string &name, int flags = 0);

        /**
         * Returns the number of open R/W transactions in the environment
         *
         * @return
         */
        size_t open_transactions() const;

        /**
         * Sets/changes the LMDB environment flags
         *
         * @param flags
         * @param flag_state
         */
        Error set_flags(int flags = 0, bool flag_state = true);

        /**
         * Retrieves the LMDB environment statistics
         *
         * @return
         */
        std::tuple<Error, MDB_stat> stats() const;

        /**
         * Opens a transaction in the database
         *
         * @param readonly
         * @return
         */
        std::unique_ptr<LMDBTransaction> transaction(bool readonly = false) const;

        /**
         * Registers a new transaction in the environment
         *
         * DO NOT USE THIS METHOD DIRECTLY!
         *
         * @param txn
         */
        void transaction_register(const LMDBTransaction &txn);

        /**
         * Un-registers a transaction from the environment
         *
         * DO NOT USE THIS METHOD DIRECTLY!
         *
         * @param txn
         */
        void transaction_unregister(const LMDBTransaction &txn);

        /**
         * Retrieves the current LMDB library version
         *
         * @return [major, minor, patch]
         */
        static std::tuple<int, int, int> version();

      private:
        /**
         * Converts the bytes of memory specified into LMDB pages (rounded up)
         *
         * @param memory
         * @return
         */
        std::tuple<Error, size_t> memory_to_pages(size_t memory) const;

        std::string m_id;

        size_t m_growth_factor;

        MDB_env *m_env;

        mutable std::mutex m_mutex, m_txn_mutex;

        std::map<std::string, std::shared_ptr<LMDBDatabase>> m_databases;

        size_t m_open_txns;
    };

    /**
     * Provides a Database model for use within an LMDB environment
     */
    class LMDBDatabase
    {
      public:
        /**
         * Opens the database within the specified environment
         *
         * DO NOT CALL THIS METHOD DIRECTLY!
         *
         * @param env
         * @param name
         * @param flags
         */
        LMDBDatabase(std::shared_ptr<LMDB> &env, const std::string &name = "", int flags = MDB_CREATE);

        ~LMDBDatabase();

        operator MDB_dbi &();

        /**
         * Returns how many key/value pairs currently exist in the database
         *
         * @return
         */
        size_t count();

        /**
         * Simplified deletion of the given key and its value. Automatically opens a
         * transaction, deletes the key, and commits the transaction, then returns.
         *
         * If we encounter MDB_MAP_FULL, we will automatically retry the transaction after
         * attempting to expand the database
         *
         * @param key
         * @return
         */
        Error del(const serializer_t &key);

        /**
         * Simplified deletion of the given key and its value. Automatically opens a
         * transaction, deletes the key, and commits the transaction, then returns.
         *
         * If we encounter MDB_MAP_FULL, we will automatically retry the transaction after
         * attempting to expand the database
         *
         * @tparam Key
         * @param key
         * @return
         */
        template<typename Key> Error del(const Key &key)
        {
            serializer_t i_key;

            key.serialize(i_key);

            return del(i_key);
        }

        /**
         * Simplified deletion of the given key and its value. Automatically opens a
         * transaction, deletes the key, and commits the transaction, then returns.
         *
         * If we encounter MDB_MAP_FULL, we will automatically retry the transaction after
         * attempting to expand the database
         *
         * @param key
         * @return
         */
        Error del(const uint64_t &key);

        /**
         * Simplified deletion of the given key with the given value. Automatically
         * opens a transaction, deletes the value, and commits the transaction, then
         * returns.
         *
         * If we encounter MDB_MAP_FULL, we will automatically retry the transaction after
         * attempting to expand the database
         *
         * @param key
         * @param value
         * @return
         */
        Error del(const serializer_t &key, const serializer_t &value);

        /**
         * Simplified deletion of the given key with the given value. Automatically
         * opens a transaction, deletes the value, and commits the transaction, then
         * returns.
         *
         * If we encounter MDB_MAP_FULL, we will automatically retry the transaction after
         * attempting to expand the database
         *
         * @tparam Key
         * @tparam Value
         * @param key
         * @param value
         * @return
         */
        template<typename Key, typename Value> Error del(const Key &key, const Value &value)
        {
            serializer_t i_key, i_value;

            key.serialize(i_key);

            value.serialize(i_value);

            return del(i_key, i_value);
        }

        /**
         * Simplified deletion of the given key with the given value. Automatically
         * opens a transaction, deletes the value, and commits the transaction, then
         * returns.
         *
         * If we encounter MDB_MAP_FULL, we will automatically retry the transaction after
         * attempting to expand the database
         *
         * @param key
         * @param value
         * @return
         */
        Error del(const uint64_t &key, const serializer_t &value);

        /**
         * Simplified deletion of the given key with the given value. Automatically
         * opens a transaction, deletes the value, and commits the transaction, then
         * returns.
         *
         * If we encounter MDB_MAP_FULL, we will automatically retry the transaction after
         * attempting to expand the database
         *
         * @tparam Value
         * @param key
         * @param value
         * @return
         */
        template<typename Value> Error del(const uint64_t &key, const Value &value)
        {
            serializer_t i_value;

            value.serialize(i_value);

            return del(key, i_value);
        }

        /**
         * Empties all of the key/value pairs from the database
         *
         * @param delete_db if specified, also deletes the database from the environment
         * @return
         */
        Error drop(bool delete_db);

        /**
         * Returns the current LMDB environment associated with this database
         *
         * @return
         */
        std::shared_ptr<LMDB> env() const;

        /**
         * Returns if the given key exists in the database
         *
         * @param key
         * @return
         */
        bool exists(const serializer_t &key);

        /**
         * Returns if the given key exists in the database
         *
         * @tparam Key
         * @param key
         * @return
         */
        template<typename Key> bool exists(const Key &key)
        {
            serializer_t i_key;

            key.serialize(i_key);

            return exists(i_key);
        }

        /**
         * Returns if the given key exists in the database
         *
         * @param key
         * @return
         */
        bool exists(const uint64_t &key);

        /**
         * Simplified retrieval of the value at the specified key which opens a new
         * readonly transaction, retrieves the value, and then returns it as the
         * specified type
         *
         * @param key
         * @return
         */
        std::tuple<Error, deserializer_t> get(const serializer_t &key);

        /**
         * Simplified retrieval of the value at the specified key which opens a new
         * readonly transaction, retrieves the value, and then returns it as the
         * specified type
         *
         * @tparam Key
         * @param key
         * @return
         */
        template<typename Key> std::tuple<Error, deserializer_t> get(const Key &key)
        {
            serializer_t i_key;

            key.serialize(i_key);

            auto [error, i_value] = get(i_key);

            if (error)
            {
                return {error, {}};
            }

            return {error, i_value};
        }

        /**
         * Simplified retrieval of the value at the specified key which opens a new
         * readonly transaction, retrieves the value, and then returns it as the
         * specified type
         *
         * @tparam Value
         * @tparam Key
         * @param key
         * @return
         */
        template<typename Value, typename Key> std::tuple<Error, Value> get(const Key &key)
        {
            serializer_t i_key;

            key.serialize(i_key);

            auto [error, i_value] = get(i_key);

            if (error)
            {
                return {error, {}};
            }

            Value value;

            value.deserialize(i_value);

            return {error, value};
        }

        /**
         * Simplified retrieval of the value at the specified key which opens a new
         * readonly transaction, retrieves the value, and then returns it as the
         * specified type
         *
         * @param key
         * @return
         */
        std::tuple<Error, deserializer_t> get(const uint64_t &key);

        /**
         * Simplified retrieval of the value at the specified key which opens a new
         * readonly transaction, retrieves the value, and then returns it as the
         * specified type
         *
         * @tparam Value
         * @param key
         * @return
         */
        template<typename Value> std::tuple<Error, Value> get(const uint64_t &key)
        {
            auto [error, i_value] = get(key);

            if (error)
            {
                return {error, {}};
            }

            Value value;

            value.deserialize(i_value);

            return {error, value};
        }

        /**
         * Simplifies retrieval of all values for all keys in the database
         *
         * WARNING: Very likely slow with large key sets
         *
         * @return
         */
        std::vector<deserializer_t> get_all();

        /**
         * Simplifies retrieval of all values for all keys in the database
         *
         * WARNING: Very likely slow with large key sets
         *
         * @tparam Value
         * @return
         */
        template<typename Value> std::vector<Value> get_all()
        {
            std::vector<Value> results;

            auto all = get_all();

            for (auto &reader : all)
            {
                Value value;

                value.deserialize(reader);

                results.push_back(value);
            }

            return results;
        }

        /**
         * Retrieves the database flags
         *
         * @return
         */
        std::tuple<Error, unsigned int> get_flags();

        /**
         * Returns the ID of the database
         *
         * @return
         */
        std::string id() const;

        /**
         * Lists all keys in the database
         *
         * @param ignore_duplicates
         * @return
         */
        std::vector<deserializer_t> list_keys(bool ignore_duplicates = true);

        /**
         * Lists all keys in the database
         *
         * @tparam Key
         * @param ignore_duplicates
         * @return
         */
        template<typename Key> std::vector<Key> list_keys(bool ignore_duplicates = true)
        {
            std::vector<Key> results;

            auto all = list_keys(ignore_duplicates);

            for (auto &reader : all)
            {
                Key key;

                key.deserialize(reader);

                results.push_back(key);
            }

            return results;
        }

        /**
         * Simplified put which opens a new transaction, puts the value, and then returns.
         *
         * If we encounter MDB_MAP_FULL, we will automatically retry the transaction after
         * attempting to expand the database
         *
         * @param key
         * @param value
         * @param flags
         * @return
         */
        Error put(const serializer_t &key, const serializer_t &value, int flags = 0);

        /**
         * Simplified put which opens a new transaction, puts the value, and then returns.
         *
         * If we encounter MDB_MAP_FULL, we will automatically retry the transaction after
         * attempting to expand the database
         *
         * @param key
         * @param flags
         * @return
         */
        template<typename Key> Error put(const Key &key, int flags = 0)
        {
            serializer_t i_key, i_value;

            key.serializer(i_key);

            return put(i_key, i_value, flags);
        }

        /**
         * Simplified put which opens a new transaction, puts the value, and then returns.
         *
         * If we encounter MDB_MAP_FULL, we will automatically retry the transaction after
         * attempting to expand the database
         *
         * @tparam Key
         * @tparam Value
         * @param key
         * @param value
         * @param flags
         * @return
         */
        template<typename Key, typename Value> Error put(const Key &key, const Value &value, int flags = 0)
        {
            serializer_t i_key, i_value;

            key.serialize(i_key);

            value.serialize(i_value);

            return put(i_key, i_value, flags);
        }

        /**
         * Simplified put which opens a new transaction, puts the value, and then returns.
         *
         * If we encounter MDB_MAP_FULL, we will automatically retry the transaction after
         * attempting to expand the database
         *
         * @param key
         * @param value
         * @param flags
         * @return
         */
        Error put(const uint64_t &key, const serializer_t &value, int flags = 0);

        /**
         * Simplified put which opens a new transaction, puts the value, and then returns.
         *
         * If we encounter MDB_MAP_FULL, we will automatically retry the transaction after
         * attempting to expand the database
         *
         * @tparam Value
         * @param key
         * @param value
         * @return
         */
        template<typename Value> Error put(const uint64_t &key, const Value &value, int flags = 0)
        {
            serializer_t i_value;

            value.serialize(i_value);

            return put(key, i_value, flags);
        }

        /**
         * Opens a transaction in the database
         *
         * @param readonly
         * @return
         */
        std::unique_ptr<LMDBTransaction> transaction(bool readonly = false);

      private:
        std::string m_id;

        std::shared_ptr<LMDB> m_env;

        MDB_dbi m_dbi;

        mutable std::mutex m_db_mutex;
    };

    /**
     * Provides a transaction model for use within a LMDB database
     *
     * Please note: A transaction will abort() automatically if it has not been committed before
     * it leaves the scope it was created in. This helps to maintain database integrity as if
     * your work in pushing to a transaction fails and throws, the transaction will always be
     * reverted.
     *
     */
    class LMDBTransaction
    {
      public:
        /**
         * Constructs a new transaction in the environment specified
         *
         * DO NOT CALL THIS METHOD DIRECTLY!
         *
         * @param env
         * @param readonly
         */
        LMDBTransaction(std::shared_ptr<LMDB> &env, bool readonly = false);

        /**
         * Constructs a new transaction in the environment and database specified
         *
         * DO NOT CALL THIS METHOD DIRECTLY!
         *
         * @param env
         * @param db
         * @param readonly
         */
        LMDBTransaction(std::shared_ptr<LMDB> &env, std::shared_ptr<LMDBDatabase> &db, bool readonly = false);

        ~LMDBTransaction();

        operator MDB_txn *&();

        /**
         * Aborts the currently open transaction
         */
        void abort();

        /**
         * Commits the currently open transaction
         *
         * @return
         */
        Error commit();

        /**
         * Opens a LMDB cursor within the transaction
         *
         * @return
         */
        std::unique_ptr<LMDBCursor> cursor();

        /**
         * Returns the transaction environment
         *
         * @return
         */
        [[nodiscard]] std::shared_ptr<LMDB> env() const;

        /**
         * Deletes the provided key
         *
         * @param key
         * @return
         */
        Error del(const serializer_t &key);

        /**
         * Deletes the provided key
         *
         * @tparam Key
         * @param key
         * @return
         */
        template<typename Key> Error del(const Key &key)
        {
            serializer_t i_key;

            key.serialize(i_key);

            return del(i_key);
        }

        /**
         * Deletes the provided key with the provided value.
         *
         * If the database supports sorted duplicates and the data parameter is NULL, all of the duplicate data items
         * for the key will be deleted. Otherwise, if the data parameter is non-NULL only the matching data item will
         * be deleted.
         *
         * @param key
         * @param value
         * @return
         */
        Error del(const serializer_t &key, const serializer_t &value);

        /**
         * Deletes the provided key with the provided value.
         *
         * If the database supports sorted duplicates and the data parameter is NULL, all of the duplicate data items
         * for the key will be deleted. Otherwise, if the data parameter is non-NULL only the matching data item will
         * be deleted.
         *
         * @tparam Key
         * @tparam Value
         * @param key
         * @param value
         * @return
         */
        template<typename Key, typename Value> Error del(const Key &key, const Value &value)
        {
            serializer_t i_key, i_value;

            key.serialize(i_key);

            value.serialize(i_value);

            return del(i_key, i_value);
        }

        /**
         * Deletes the provided key
         *
         * @param key
         * @return
         */
        Error del(const uint64_t &key);

        /**
         * Deletes the provided key with the provided value.
         *
         * If the database supports sorted duplicates and the data parameter is NULL, all of the duplicate data items
         * for the key will be deleted. Otherwise, if the data parameter is non-NULL only the matching data item will
         * be deleted.
         *
         * @param key
         * @param value
         * @return
         */
        Error del(const uint64_t &key, const serializer_t &value);

        /**
         * Deletes the provided key with the provided value.
         *
         * If the database supports sorted duplicates and the data parameter is NULL, all of the duplicate data items
         * for the key will be deleted. Otherwise, if the data parameter is non-NULL only the matching data item will
         * be deleted.
         *
         * @tparam Value
         * @param key
         * @param value
         * @return
         */
        template<typename Value> Error del(const uint64_t &key, const Value &value)
        {
            serializer_t i_value;

            value.serialize(i_value);

            return del(key, i_value);
        }

        /**
         * Checks if the given key exists in the database
         *
         * @param key
         * @return
         */
        bool exists(const serializer_t &key);

        /**
         * Checks if the given key exists in the database
         *
         * @tparam Key
         * @param key
         * @return
         */
        template<typename Key> bool exists(const Key &key)
        {
            serializer_t i_key;

            key.serialize(i_key);

            return exists(i_key);
        }

        /**
         * Checks if the given key exists in the database
         *
         * @param key
         * @return
         */
        bool exists(const uint64_t &key);

        /**
         * Retrieves the value stored with the specified key
         *
         * @param key
         * @return
         */
        std::tuple<Error, deserializer_t> get(const serializer_t &key);

        /**
         * Retrieves the value stored with the specified key
         *
         * @tparam Key
         * @param key
         * @return
         */
        template<typename Key> std::tuple<Error, deserializer_t> get(const Key &key)
        {
            serializer_t i_key;

            key.serialize(i_key);

            return get(i_key);
        }

        /**
         * Retrieves the value stored with the specified key
         *
         * @tparam Key
         * @tparam Value
         * @param key
         * @return
         */
        template<typename Value, typename Key> std::tuple<Error, Value> get(const Key &key)
        {
            serializer_t i_key;

            key.serialize(i_key);

            auto [error, i_value] = get(i_key);

            if (error)
            {
                return {error, {}};
            }

            Value value;

            value.deserialize(i_value);

            return {error, value};
        }

        /**
         * Retrieves the value stored with the specified key
         *
         * @param key
         * @return
         */
        std::tuple<Error, deserializer_t> get(const uint64_t &key);

        /**
         * Returns the transaction ID
         *
         * If a 0 value is returned, the transaction is complete [abort() or commit() has been used]
         *
         * @return
         */
        [[nodiscard]] std::tuple<Error, size_t> id() const;

        /**
         * Puts the specified value with the specified key in the database using the specified flag(s)
         *
         * Note: You must check for MDB_MAP_FULL or MDB_TXN_FULL response values and handle those
         * yourself as you will very likely need to abort the current transaction and expand
         * the LMDB environment before re-attempting the transaction.
         *
         * @param key
         * @param value
         * @param flags
         * @return
         */
        Error put(const serializer_t &key, const serializer_t &value, int flags = 0);

        /**
         * Puts the specified value with the specified key in the database using the specified flag(s)
         *
         * Note: You must check for MDB_MAP_FULL or MDB_TXN_FULL response values and handle those
         * yourself as you will very likely need to abort the current transaction and expand
         * the LMDB environment before re-attempting the transaction.
         *
         * @tparam Key
         * @param key
         * @param flags
         * @return
         */
        template<typename Key> Error put(const Key &key, int flags = 0)
        {
            serializer_t i_key, i_value;

            key.serialize(i_key);

            return put(i_key, i_value, flags);
        }

        /**
         * Puts the specified value with the specified key in the database using the specified flag(s)
         *
         * Note: You must check for MDB_MAP_FULL or MDB_TXN_FULL response values and handle those
         * yourself as you will very likely need to abort the current transaction and expand
         * the LMDB environment before re-attempting the transaction.
         *
         * @tparam Key
         * @tparam Value
         * @param key
         * @param value
         * @param flags
         * @return
         */
        template<typename Key, typename Value> Error put(const Key &key, const Value &value, int flags = 0)
        {
            serializer_t i_key, i_value;

            key.serialize(i_key);

            value.serialize(i_value);

            return put(i_key, i_value, flags);
        }

        /**
         * Puts the specified value with the specified key in the database using the specified flag(s)
         *
         * Note: You must check for MDB_MAP_FULL or MDB_TXN_FULL response values and handle those
         * yourself as you will very likely need to abort the current transaction and expand
         * the LMDB environment before re-attempting the transaction.
         *
         * @param key
         * @param value
         * @param flags
         * @return
         */
        Error put(const uint64_t &key, const serializer_t &value, int flags = 0);

        /**
         * Puts the specified value with the specified key in the database using the specified flag(s)
         *
         * Note: You must check for MDB_MAP_FULL or MDB_TXN_FULL response values and handle those
         * yourself as you will very likely need to abort the current transaction and expand
         * the LMDB environment before re-attempting the transaction.
         *
         * @tparam Value
         * @param key
         * @param value
         * @param flags
         * @return
         */
        template<typename Value> Error put(const uint64_t &key, const Value &value, int flags = 0)
        {
            serializer_t i_value;

            value.serialize(i_value);

            return put(key, i_value, flags);
        }

        /**
         * Returns if the transaction is readonly or not
         *
         * @return
         */
        [[nodiscard]] bool readonly() const;

        /**
         * Renew a read-only transaction that has been previously reset()
         *
         * @return
         */
        Error renew();

        /**
         * Reset a read-only transaction.
         */
        Error reset();

        /**
         * Sets the current database for the transaction
         *
         * @param db
         */
        void set_database(std::shared_ptr<LMDBDatabase> &db);

      private:
        /**
         * Sets up the transaction via the multiple entry methods
         */
        void txn_setup();

        std::shared_ptr<LMDB> m_env;

        std::shared_ptr<LMDBDatabase> m_db;

        std::shared_ptr<MDB_txn *> m_txn;

        bool m_readonly;
    };

    /**
     * Provies a Cursor model for use within a LMDB transaction
     */
    class LMDBCursor
    {
      public:
        /**
         * Creates a new LMDB Cursor within the specified transaction and database
         *
         * DO NOT CALL THIS METHOD DIRECTLY!
         *
         * @param txn
         * @param db
         * @param readonly
         */
        LMDBCursor(std::shared_ptr<MDB_txn *> &txn, std::shared_ptr<LMDBDatabase> &db, bool readonly = false);

        ~LMDBCursor();

        operator MDB_cursor *&();

        /**
         * Return count of duplicates for current key.
         *
         * @return
         */
        std::tuple<Error, size_t> count();

        /**
         * Delete current key/data pair.
         *
         * @param flags
         * @return
         */
        Error del(int flags = 0);

        /**
         * Retrieve key/value pairs by cursor.
         *
         * @param op
         * @return
         */
        std::tuple<Error, deserializer_t, deserializer_t> get(const MDB_cursor_op &op = MDB_FIRST);

        /**
         * Retrieve key/value pairs by cursor.
         *
         * @tparam Key
         * @param op
         * @return
         */
        template<typename Key> std::tuple<Error, Key, deserializer_t> get(const MDB_cursor_op &op = MDB_FIRST)
        {
            auto [error, i_key, i_value] = get(op);

            if (error)
            {
                return {error, {}, {}};
            }

            Key key;

            key.deserialize(i_key);

            return {error, key, i_value};
        }

        /**
         * Retrieve key/value pairs by cursor.
         *
         * @tparam Key
         * @tparam Value
         * @param op
         * @return
         */
        template<typename Key, typename Value> std::tuple<Error, Key, Value> get(const MDB_cursor_op &op = MDB_FIRST)
        {
            auto [error, i_key, i_value] = get(op);

            if (error)
            {
                return {error, {}, {}};
            }

            Key key;

            key.deserialize(i_key);

            Value value;

            value.deserialize(i_value);

            return {error, key, value};
        }

        /**
         * Retrieve key/value pairs by cursor.
         *
         * Providing the key, allows for utilizing additional MDB_cursor_op values such as MDB_SET which will
         * retrieve the value for the specified key without changing the key value.
         *
         * @param key
         * @param op
         * @return
         */
        std::tuple<Error, deserializer_t, deserializer_t>
            get(const serializer_t &key, const MDB_cursor_op &op = MDB_SET);

        /**
         * Retrieve key/value pairs by cursor.
         *
         * Providing the key, allows for utilizing additional MDB_cursor_op values such as MDB_SET which will
         * retrieve the value for the specified key without changing the key value.
         *
         * @tparam Key
         * @param key
         * @param op
         * @return
         */
        template<typename Key>
        std::tuple<Error, Key, deserializer_t> get(const Key &key, const MDB_cursor_op &op = MDB_SET)
        {
            serializer_t i_key;

            key.serialize(i_key);

            auto [error, r_key, f_value] = get(i_key, op);

            if (error)
            {
                return {error, {}, {}};
            }

            Key f_key;

            f_key.deserialize(r_key);

            return {error, f_key, f_value};
        }

        /**
         * Retrieve key/value pairs by cursor.
         *
         * Providing the key, allows for utilizing additional MDB_cursor_op values such as MDB_SET which will
         * retrieve the value for the specified key without changing the key value.
         *
         * @tparam Value
         * @tparam Key
         * @param key
         * @param op
         * @return
         */
        template<typename Value, typename Key>
        std::tuple<Error, Key, Value> get(const Key &key, const MDB_cursor_op &op = MDB_SET)
        {
            serializer_t i_key;

            key.serialize(i_key);

            auto [error, r_key, r_value] = get(i_key, op);

            if (error)
            {
                return {error, {}, {}};
            }

            Key f_key;

            f_key.deserialize(r_key);

            Value f_value;

            f_value.deserialize(r_value);

            return {error, f_key, f_value};
        }

        /**
         * Retrieve key/value pairs by cursor.
         *
         * Providing the key, allows for utilizing additional MDB_cursor_op values such as MDB_SET which will
         * retrieve the value for the specified key without changing the key value.
         *
         * @param key
         * @param op
         * @return
         */
        std::tuple<Error, uint64_t, deserializer_t> get(const uint64_t &key, const MDB_cursor_op &op = MDB_SET);

        /**
         * Retrieve key/value pairs by cursor.
         *
         * Providing the key, allows for utilizing additional MDB_cursor_op values such as MDB_SET which will
         * retrieve the value for the specified key without changing the key value.
         *
         * @tparam Value
         * @param key
         * @param op
         * @return
         */
        template<typename Value>
        std::tuple<Error, uint64_t, Value> get(const uint64_t &key, const MDB_cursor_op &op = MDB_SET)
        {
            auto [error, f_key, r_value] = get(key, op);

            if (error)
            {
                return {error, {}, {}};
            }

            Value value;

            value.deserialize(r_value);

            return {error, f_key, value};
        }

        /**
         * Retrieve multiple values for a single key from the database
         *
         * Requires that MDB_DUPSORT was used when opening the database
         *
         * @param key
         * @return
         */
        std::tuple<Error, deserializer_t, std::vector<deserializer_t>> get_all(const serializer_t &key);

        /**
         * Retrieve multiple values for a single key from the database
         *
         * Requires that MDB_DUPSORT was used when opening the database
         *
         * @tparam Key
         * @param key
         * @return
         */
        template<typename Key> std::tuple<Error, Key, std::vector<deserializer_t>> get_all(const Key &key)
        {
            serializer_t i_key;

            key.serialize(key);

            return get_all(i_key);
        }

        /**
         * Retrieve multiple values for a single key from the database
         *
         * Requires that MDB_DUPSORT was used when opening the database
         *
         * @tparam Value
         * @tparam Key
         * @param key
         * @return
         */
        template<typename Value, typename Key> std::tuple<Error, Key, std::vector<Value>> get_all(const Key &key)
        {
            serializer_t i_key;

            key.serialize(i_key);

            auto [error, r_key, r_values] = get_all(i_key);

            if (error)
            {
                return {error, {}, {}};
            }

            Key f_key;

            f_key.deserialize(r_key);

            std::vector<Value> results;

            for (auto &reader : r_values)
            {
                Value r_value;

                r_value.deserialize(reader);

                results.push_back(r_value);
            }

            return {error, f_key, results};
        }

        /**
         * Retrieve multiple values for a single key from the database
         *
         * Requires that MDB_DUPSORT was used when opening the database
         *
         * @param key
         * @return
         */
        std::tuple<Error, uint64_t, std::vector<deserializer_t>> get_all(const uint64_t &key);

        /**
         * Retrieve multiple values for a single key from the database
         *
         * Requires that MDB_DUPSORT was used when opening the database
         *
         * @tparam Value
         * @param key
         * @return
         */
        template<typename Value> std::tuple<Error, uint64_t, std::vector<Value>> get_all(const uint64_t &key)
        {
            auto [error, f_key, r_values] = get_all(key);

            if (error)
            {
                return {error, {}, {}};
            }

            std::vector<Value> results;

            for (auto &reader : r_values)
            {
                Value r_value;

                r_value.deserialize(reader);

                results.push_back(r_value);
            }

            return {error, f_key, results};
        };

        /**
         * Puts the specified value with the specified key in the database using the specified flag(s)
         * and places the cursor at the position of the new item or, near it upon failure.
         *
         * Note: You must check for MDB_MAP_FULL or MDB_TXN_FULL response values and handle those
         * yourself as you will very likely need to abort the current transaction and expand
         * the LMDB environment before re-attempting the transaction.
         *
         * @param key
         * @param value
         * @param flags
         * @return
         */
        Error put(const serializer_t &key, const serializer_t &value, int flags = 0);

        /**
         * Puts the specified value with the specified key in the database using the specified flag(s)
         * and places the cursor at the position of the new item or, near it upon failure.
         *
         * Note: You must check for MDB_MAP_FULL or MDB_TXN_FULL response values and handle those
         * yourself as you will very likely need to abort the current transaction and expand
         * the LMDB environment before re-attempting the transaction.
         *
         * @tparam Key
         * @param key
         * @param flags
         * @return
         */
        template<class Key> Error put(const Key &key, int flags)
        {
            serializer_t i_key, i_value;

            key.serialize(i_key);

            return put(i_key, i_value, flags);
        }

        /**
         * Puts the specified value with the specified key in the database using the specified flag(s)
         * and places the cursor at the position of the new item or, near it upon failure.
         *
         * Note: You must check for MDB_MAP_FULL or MDB_TXN_FULL response values and handle those
         * yourself as you will very likely need to abort the current transaction and expand
         * the LMDB environment before re-attempting the transaction.
         *
         * @tparam Key
         * @param key
         * @param value
         * @param flags
         * @return
         */
        template<typename Key> Error put(const Key &key, const serializer_t &value, int flags = 0)
        {
            serializer_t i_key;

            key.serialize(i_key);

            return put(i_key, value, flags);
        }

        /**
         * Puts the specified value with the specified key in the database using the specified flag(s)
         * and places the cursor at the position of the new item or, near it upon failure.
         *
         * Note: You must check for MDB_MAP_FULL or MDB_TXN_FULL response values and handle those
         * yourself as you will very likely need to abort the current transaction and expand
         * the LMDB environment before re-attempting the transaction.
         *
         * @tparam Key
         * @tparam Value
         * @param key
         * @param value
         * @param flags
         * @return
         */
        template<typename Key, typename Value> Error put(const Key &key, const Value &value, int flags = 0)
        {
            serializer_t i_key, i_value;

            key.serialize(i_key);

            value.serialize(i_value);

            return put(i_key, i_value, flags);
        }

        /**
         * Puts the specified value with the specified key in the database using the specified flag(s)
         * and places the cursor at the position of the new item or, near it upon failure.
         *
         * Note: You must check for MDB_MAP_FULL or MDB_TXN_FULL response values and handle those
         * yourself as you will very likely need to abort the current transaction and expand
         * the LMDB environment before re-attempting the transaction.
         *
         * @param key
         * @param value
         * @param flags
         * @return
         */
        Error put(const uint64_t &key, const serializer_t &value, int flags = 0);

        /**
         * Puts the specified value with the specified key in the database using the specified flag(s)
         * and places the cursor at the position of the new item or, near it upon failure.
         *
         * Note: You must check for MDB_MAP_FULL or MDB_TXN_FULL response values and handle those
         * yourself as you will very likely need to abort the current transaction and expand
         * the LMDB environment before re-attempting the transaction.
         *
         * @tparam Value
         * @param key
         * @param value
         * @param flags
         * @return
         */
        template<typename Value> Error put(const uint64_t &key, const Value &value, int flags = 0)
        {
            serializer_t i_value;

            value.serialize(i_value);

            return put(key, i_value, flags);
        }

        /**
         * Renews the cursor
         *
         * @return
         */
        Error renew();

      private:
        std::shared_ptr<LMDBDatabase> m_db;

        std::shared_ptr<MDB_txn *> m_txn;

        MDB_cursor *m_cursor = nullptr;

        bool m_readonly;
    };
} // namespace Database

#endif // DATABASE_LMDB_H
