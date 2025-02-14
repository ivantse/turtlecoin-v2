// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_P2P_DATABASE_H
#define TURTLECOIN_P2P_DATABASE_H

#include <crypto_types.h>
#include <db_lmdb.h>
#include <logger.h>
#include <types.h>

using namespace Types::Network;

namespace P2P
{
    class PeerDB
    {
      protected:
        /**
         * Creates a new instance of the class
         *
         * @param logger
         * @param path
         */
        PeerDB(logger &logger, const std::string &path);

      public:
        ~PeerDB();

        /**
         * Adds the peer entry to the database
         *
         * @param entry
         * @return
         */
        Error add(const network_peer_t &entry);

        /**
         * Returns the total count of entries in the database
         *
         * @return
         */
        size_t count() const;

        /**
         * Deletes the provided peer entry from the database
         *
         * @param entry
         * @return
         */
        Error del(const network_peer_t &entry);

        /**
         * Deletes the peer with the given ID from the database
         *
         * @param peer_id
         * @return
         */
        Error del(const crypto_hash_t &peer_id);

        /**
         * Returns if the peer ID exists in the database
         *
         * @param peer_id
         * @return
         */
        bool exists(const crypto_hash_t &peer_id) const;

        /**
         * Retrieve the given peer entry for the specified peer ID
         *
         * @param peer_id
         * @return
         */
        std::tuple<Error, network_peer_t> get(const crypto_hash_t &peer_id);

        /**
         * Retrieves a singleton instance of the class
         *
         * @param logger
         * @param path
         * @return
         */
        static std::shared_ptr<PeerDB> instance(logger &logger, const std::string &path);

        /**
         * Returns our peer ID
         *
         * @return
         */
        crypto_hash_t peer_id() const;

        /**
         * Returns a list of all peer IDs in the database
         *
         * @return
         */
        std::vector<crypto_hash_t> peer_ids() const;

        /**
         * Returns peers in the database in a random order
         *
         * NOTE: Specifying a count of 0 will return all peers while supplying
         * a non-zero count will return that many peers if available.
         *
         * NOTE: If the network ID is specified, only peers from that
         * network ID will be returned
         *
         * @param count
         * @param network_id
         * @return
         */
        std::vector<network_peer_t> peers(size_t count = 0, const crypto_hash_t &network_id = crypto_hash_t()) const;

        /**
         * Prunes peers from the database that have not been seen in the last
         * configured amount of time
         */
        void prune();

        /**
         * Touches a peer's last seen time in the database updating it to NOW
         *
         * @param peer_id
         * @return
         */
        Error touch(const crypto_hash_t &peer_id);

      private:
        std::shared_ptr<Database::LMDB> m_env;

        std::shared_ptr<Database::LMDBDatabase> m_database;

        mutable std::mutex m_mutex;

        crypto_hash_t m_id, m_peer_id;

        logger m_logger;
    };
} // namespace P2P

#endif
