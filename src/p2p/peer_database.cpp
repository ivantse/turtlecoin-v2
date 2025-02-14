// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "peer_database.h"

#include <algorithm>
#include <random>

// static entry that we can use to lookup our own peer id in the database
static const auto PEER_ID_IDENTIFIER =
    crypto_hash_t("5440dd9b6683e3b2b0805eec3514ff3e23b7edea1bf29b434cd7a8447687650d");

static ThreadSafeMap<crypto_hash_t, std::shared_ptr<P2P::PeerDB>> instances;

namespace P2P
{
    PeerDB::PeerDB(logger &logger, const std::string &path): m_logger(logger)
    {
        m_id = Crypto::Hashing::sha3(path.data(), path.size());

        m_env = Database::LMDB::instance(path);

        m_database = m_env->open_database("peerlist");

        /**
         * try to retrieve our already generated peer id from the database
         * otherwise generate a new one and stick it in the database
         */
        auto info = m_env->open_database("local");

        const auto [error, value] = info->get<crypto_hash_t, crypto_hash_t>(PEER_ID_IDENTIFIER);

        if (!error)
        {
            m_peer_id = value;
        }
        else
        {
            m_peer_id = Crypto::random_hash();

            m_logger->debug("Generated new peer ID: {0}", m_peer_id.to_string());
        }

        info->put(PEER_ID_IDENTIFIER, m_peer_id);
    }

    PeerDB::~PeerDB()
    {
        if (instances.contains(m_id))
        {
            instances.erase(m_id);
        }
    }

    Error PeerDB::add(const network_peer_t &entry)
    {
        if (entry.peer_id == m_peer_id)
        {
            return MAKE_ERROR_MSG(PEERLIST_ADD_FAILURE, "Error adding self to peer database.");
        }

        const auto prune_time = (time(nullptr)) - Configuration::P2P::PEER_PRUNE_TIME;

        if (entry.last_seen < prune_time)
        {
            return MAKE_ERROR_MSG(PEERLIST_ADD_FAILURE, "Peer last seen too far in the past.");
        }

        std::scoped_lock lock(m_mutex);

        m_logger->trace("Adding new Peer entry: {0}", entry.peer_id.to_string());

        return m_database->put(entry.peer_id, entry);
    }

    size_t PeerDB::count() const
    {
        std::scoped_lock lock(m_mutex);

        return m_database->count();
    }

    Error PeerDB::del(const network_peer_t &entry)
    {
        std::scoped_lock lock(m_mutex);

        m_logger->trace("Deleting Peer entry: {0}", entry.peer_id.to_string());

        return m_database->del(entry.peer_id);
    }

    Error PeerDB::del(const crypto_hash_t &peer_id)
    {
        std::scoped_lock lock(m_mutex);

        m_logger->trace("Deleting Peer entry: {0}", peer_id.to_string());

        return m_database->del(peer_id);
    }

    bool PeerDB::exists(const crypto_hash_t &peer_id) const
    {
        std::scoped_lock lock(m_mutex);

        return m_database->exists(peer_id);
    }

    std::tuple<Error, network_peer_t> PeerDB::get(const crypto_hash_t &peer_id)
    {
        std::scoped_lock lock(m_mutex);

        return m_database->get<network_peer_t>(peer_id);
    }

    std::shared_ptr<PeerDB> PeerDB::instance(logger &logger, const std::string &path)
    {
        const auto id = Crypto::Hashing::sha3(path.data(), path.size());

        if (!instances.contains(id))
        {
            auto db = new PeerDB(logger, path);

            auto ptr = std::shared_ptr<PeerDB>(db);

            instances.insert(id, ptr);
        }

        return instances.at(id);
    }

    crypto_hash_t PeerDB::peer_id() const
    {
        return m_peer_id;
    }

    std::vector<crypto_hash_t> PeerDB::peer_ids() const
    {
        std::scoped_lock lock(m_mutex);

        return m_database->list_keys<crypto_hash_t>();
    }

    std::vector<network_peer_t> PeerDB::peers(size_t count, const crypto_hash_t &network_id) const
    {
        std::scoped_lock lock(m_mutex);

        auto peers = m_database->get_all<network_peer_t>();

        /**
         * We were asked for peers for a specific network ID
         * then we need to filter the results to just those
         * peers that are participating in that network ID
         */
        if (!network_id.empty())
        {
            std::vector<network_peer_t> temp;

            std::copy_if(
                peers.begin(),
                peers.end(),
                std::back_inserter(temp),
                [=](const auto &elem) { return elem.network_id == network_id; });

            peers = temp;
        }

        const auto seed = std::chrono::system_clock::now().time_since_epoch().count();

        // shuffle the peers around that we received using a random iterator
        std::shuffle(peers.begin(), peers.end(), std::default_random_engine(seed));

        if (count != 0 && peers.size() > count)
        {
            peers.resize(count);
        }

        return std::move(peers);
    }

    void PeerDB::prune()
    {
        const auto all_peers = peers();

        const auto prune_time = (time(nullptr)) - Configuration::P2P::PEER_PRUNE_TIME;

        if (!all_peers.empty())
        {
            m_logger->trace("Starting peer list pruning...");
        }

        for (const auto &peer : all_peers)
        {
            if (peer.last_seen < prune_time)
            {
                auto error = del(peer.peer_id);

                if (error)
                {
                    m_logger->debug("Error deleting peer {0}: {1}", peer.peer_id.to_string(), error.to_string());
                }
            }
        }
    }

    Error PeerDB::touch(const crypto_hash_t &peer_id)
    {
        auto [error_get, peer] = get(peer_id);

        if (error_get)
        {
            return error_get;
        }

        peer.last_seen = time(nullptr);

        return add(peer);
    }
} // namespace P2P
