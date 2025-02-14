// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "p2p_node.h"

#include <tools/thread_helper.h>
#include <utilities.h>

using namespace BaseTypes;
using namespace Types::Network;

namespace P2P
{
    Node::Node(
        logger &logger,
        const std::string &path,
        const uint16_t &bind_port,
        bool seed_mode,
        const crypto_hash_t &network_id):
        m_running(false), m_logger(logger), m_seed_mode(seed_mode), m_network_id(network_id)
    {
        m_peer_db = PeerDB::instance(logger, path);

        m_peer_db->prune();

        m_server = std::make_shared<Networking::ZMQServer>(m_logger, bind_port);
    }

    Node::~Node()
    {
        m_logger->debug("Shutting down P2P network node");

        m_running = false;

        m_stopping.notify_all();

        m_server.reset();

        m_clients.clear();

        m_logger->trace("Shutdown all connected clients");

        if (m_connection_manager_thread.joinable())
        {
            m_connection_manager_thread.join();

            m_logger->trace("Shut down P2P connection manager thread successfully");
        }

        if (m_poller_thread.joinable())
        {
            m_poller_thread.join();

            m_logger->trace("Shut down P2P poller thread successfully");
        }

        if (m_keepalive_thread.joinable())
        {
            m_keepalive_thread.join();

            m_logger->trace("Shut down P2P keep alive thread successfully");
        }

        if (m_peer_exchange_thread.joinable())
        {
            m_peer_exchange_thread.join();

            m_logger->trace("Shut down P2P peer exchange thread successfully");
        }

        m_logger->debug("P2P Network Node shutdown complete");
    }

    packet_handshake_t Node::build_handshake() const
    {
        packet_handshake_t packet(m_peer_db->peer_id(), m_server->port(), m_network_id);

        packet.peers = build_peer_list();

        return packet;
    }

    std::vector<network_peer_t> Node::build_peer_list() const
    {
        auto results = m_peer_db->peers();

        if (results.size() > Configuration::P2P::MAXIMUM_PEERS_EXCHANGED)
        {
            results.resize(Configuration::P2P::MAXIMUM_PEERS_EXCHANGED);
        }

        return results;
    }

    Error Node::connect(const std::string &unsafe_host, const uint16_t &unsafe_port)
    {
        const auto [host, port, hash] = Utilities::normalize_host_port(unsafe_host, unsafe_port);

        if (m_clients.contains(hash))
        {
            return MAKE_ERROR_MSG(P2P_DUPE_CONNECT, "Already connected to specified host and port");
        }

        m_logger->debug("Attempting connection to: {0} => {1}:{2}", hash.to_string(), host, port);

        auto client = std::make_shared<Networking::ZMQClient>(m_logger);

        auto error = client->connect(host, port);

        if (error)
        {
            return error;
        }

        const auto packet = build_handshake();

        auto message = zmq_message_envelope_t(packet);

        client->send(message);

        m_clients.insert(hash, client);

        return MAKE_ERROR(SUCCESS);
    }

    void Node::connection_manager()
    {
        while (m_running)
        {
            // check to see if any of our clients are disconnected, and if so, remove em
            {
                std::vector<crypto_hash_t> to_delete;

                m_clients.each(
                    [&](const auto &id, const auto &client)
                    {
                        if (!client->is_connected())
                        {
                            m_logger->trace("Client {0} is no longer connected, destroying...", id.to_string());

                            to_delete.push_back(id);
                        }
                    });

                for (const auto &id : to_delete)
                {
                    m_clients.erase(id);
                }
            }

            const auto delta_connections = Configuration::P2P::DEFAULT_CONNECTION_COUNT - outgoing_connections();

            if (delta_connections > 0)
            {
                std::vector<network_peer_t> peers;

                /**
                 * If we are running in seed mode, then we connect to peers of all networks
                 * so that we can stretch far and wide and learn about as many peers
                 * as possible
                 */
                if (m_seed_mode)
                {
                    peers = m_peer_db->peers(delta_connections);
                }
                else
                {
                    peers = m_peer_db->peers(delta_connections, m_network_id);
                }

                if (!peers.empty())
                {
                    for (const auto &peer : peers)
                    {
                        // do not connect to ourselves
                        if (peer.peer_id == m_peer_db->peer_id())
                        {
                            continue;
                        }

                        auto error = connect(peer.address.to_string(), peer.port);

                        if (error && error != P2P_DUPE_CONNECT)
                        {
                            m_logger->debug("Error connecting to peer: {0}", error.to_string());
                        }
                    }
                }
            }

            if (thread_sleep(m_stopping, Configuration::P2P::CONNECTION_MANAGER_INTERVAL))
            {
                break;
            }
        }
    }

    std::string Node::external_address() const
    {
        return m_server->external_address();
    }

    void Node::handle_incoming_message(const zmq_message_envelope_t &message, bool is_server)
    {
        try
        {
            auto reader = deserializer_t(message.payload);

            const auto type = reader.varint<uint64_t>(true);

            switch (type)
            {
                case NetworkPacketTypes::NETWORK_HANDSHAKE:
                    return handle_packet(message.from, message.peer_address, packet_handshake_t(reader), is_server);
                case NetworkPacketTypes::NETWORK_PEER_EXCHANGE:
                    return handle_packet(message.from, message.peer_address, packet_peer_exchange_t(reader), is_server);
                case NetworkPacketTypes::NETWORK_KEEPALIVE:
                    return handle_packet(message.from, message.peer_address, packet_keepalive_t(reader), is_server);
                case NetworkPacketTypes::NETWORK_DATA:
                    return handle_packet(message.from, message.peer_address, packet_data_t(reader), is_server);
                default:
                    throw std::runtime_error("Unknown packet type detected");
            }
        }
        catch (const std::exception &e)
        {
            // TODO: if we cannot handle the message, we need to disconnect whoever sent it SOMEHOW
            m_logger->trace("Could not handle incoming P2P message: {0}", e.what());
        }
    }

    void Node::handle_packet(
        const crypto_hash_t &from,
        const std::string &peer_address,
        const Types::Network::packet_handshake_t &packet,
        bool is_server)
    {
        if (is_server && m_completed_handshake.contains(from))
        {
            m_logger->trace("Handshake already completed, protocol violation: {0}", from.to_string());

            return;
        }

        // we don't talk to ourselves
        if (from == m_server->identity() || packet.peer_id == m_peer_db->peer_id())
        {
            return;
        }

        // we don't talk to peers that are not speaking at least the minimum version
        if (packet.version < Configuration::P2P::MINIMUM_VERSION)
        {
            m_logger->trace("Peer is running the wrong version of the P2P stack: {0}", from.to_string());

            return;
        }

        if (packet.peers.size() > Configuration::P2P::MAXIMUM_PEERS_EXCHANGED)
        {
            m_logger->trace(
                "Handshake contains more than the maximum number of peers accepted [{0}]: {1}",
                std::to_string(packet.peers.size()),
                from.to_string());

            return;
        }

        {
            network_peer_t peer(ip_address_t(peer_address), packet.peer_id, packet.peer_port, packet.network_id);

            m_peer_db->add(peer);
        }

        for (const auto &peer : packet.peers)
        {
            if (peer.peer_id == packet.peer_id)
            {
                continue;
            }

            m_peer_db->add(peer);
        }

        if (is_server)
        {
            const auto reply_handshake = build_handshake();

            zmq_message_envelope_t message(from, reply_handshake);

            reply(message);

            m_completed_handshake.insert(from);
        }
    }

    void Node::handle_packet(
        const crypto_hash_t &from,
        const std::string &peer_address,
        const Types::Network::packet_data_t &packet,
        bool is_server)
    {
        // if we are running in seed mode, then all data packets are ignored
        if (m_seed_mode)
        {
            return;
        }

        // if the packet is not for our network id, drop it
        if (packet.network_id != m_network_id)
        {
            return;
        }

        if (!m_completed_handshake.contains(from))
        {
            m_logger->trace("Handshake not completed first, protocol violation: {0}", from.to_string());

            return;
        }

        // we don't talk to ourselves
        if (from == m_server->identity())
        {
            return;
        }

        // we don't talk to peers that are not speaking at least the minimum version
        if (packet.version < Configuration::P2P::MINIMUM_VERSION)
        {
            m_logger->trace("Peer is running the wrong version of the P2P stack: {0}", from.to_string());

            return;
        }

        // add the message to the stack for processing
        m_messages.push(network_msg_t(from, packet, is_server));
    }

    void Node::handle_packet(
        const crypto_hash_t &from,
        const std::string &peer_address,
        const Types::Network::packet_keepalive_t &packet,
        bool is_server)
    {
        if (!is_server)
        {
            m_peer_db->touch(packet.peer_id);

            return;
        }

        if (!m_completed_handshake.contains(from))
        {
            m_logger->trace("Handshake not completed first, protocol violation: {0}", from.to_string());

            return;
        }

        // we don't talk to ourselves
        if (from == m_server->identity() || packet.peer_id == m_peer_db->peer_id())
        {
            return;
        }

        // we don't talk to peers that are not speaking at least the minimum version
        if (packet.version < Configuration::P2P::MINIMUM_VERSION)
        {
            m_logger->trace("Peer is running the wrong version of the P2P stack: {0}", from.to_string());

            return;
        }

        packet_keepalive_t reply_keepalive(m_peer_db->peer_id());

        zmq_message_envelope_t message(from, reply_keepalive);

        reply(message);

        m_peer_db->touch(packet.peer_id);
    }

    void Node::handle_packet(
        const crypto_hash_t &from,
        const std::string &peer_address,
        const Types::Network::packet_peer_exchange_t &packet,
        bool is_server)
    {
        if (is_server && !m_completed_handshake.contains(from))
        {
            m_logger->trace("Handshake not completed first, protocol violation: {0}", from.to_string());

            return;
        }

        // we don't talk to ourselves
        if (from == m_server->identity() || packet.peer_id == m_peer_db->peer_id())
        {
            return;
        }

        // we don't talk to peers that are not speaking at least the minimum version
        if (packet.version < Configuration::P2P::MINIMUM_VERSION)
        {
            m_logger->trace("Peer is running the wrong version of the P2P stack: {0}", from.to_string());

            return;
        }

        {
            network_peer_t peer(ip_address_t(peer_address), packet.peer_id, packet.peer_port, packet.network_id);

            m_peer_db->add(peer);
        }

        for (const auto &peer : packet.peers)
        {
            if (peer.peer_id == packet.peer_id)
            {
                continue;
            }

            m_peer_db->add(peer);
        }

        if (is_server)
        {
            packet_peer_exchange_t reply_peer_exchange(m_peer_db->peer_id(), m_server->port(), m_network_id);

            reply_peer_exchange.peers = build_peer_list();

            zmq_message_envelope_t message(from, reply_peer_exchange);

            reply(message);
        }
    }

    std::vector<std::string> Node::incoming_connected() const
    {
        return m_server->connected();
    }

    size_t Node::incoming_connections() const
    {
        return m_server->connections();
    }

    ThreadSafeQueue<network_msg_t> &Node::messages()
    {
        return m_messages;
    }

    std::vector<std::string> Node::outgoing_connected() const
    {
        std::vector<std::string> results;

        m_clients.each(
            [&](const crypto_hash_t &id, const std::shared_ptr<Networking::ZMQClient> &client)
            {
                for (const auto &connection : client->connected())
                {
                    results.emplace_back(connection);
                }
            });

        return results;
    }

    size_t Node::outgoing_connections() const
    {
        return m_clients.size();
    }

    crypto_hash_t Node::peer_id() const
    {
        return m_peer_db->peer_id();
    }

    std::shared_ptr<PeerDB> Node::peers() const
    {
        return m_peer_db;
    }

    uint16_t Node::port() const
    {
        return m_server->port();
    }

    void Node::poller()
    {
        while (m_running)
        {
            while (!m_server->messages().empty())
            {
                const auto message = m_server->messages().pop();

                handle_incoming_message(message, true);
            }

            m_clients.each(
                [&](const auto &id, const auto &client)
                {
                    while (!client->messages().empty())
                    {
                        const auto message = client->messages().pop();

                        handle_incoming_message(message, false);
                    }
                });

            if (thread_sleep(m_stopping))
            {
                break;
            }
        }
    }

    void Node::reply(const zmq_message_envelope_t &message)
    {
        m_server->send(message);
    }

    void Node::reply(const crypto_hash_t &to, const packet_data_t &packet)
    {
        m_server->send(zmq_message_envelope_t(to, packet.serialize()));
    }

    bool Node::running() const
    {
        return m_running;
    }

    void Node::send(const zmq_message_envelope_t &message)
    {
        m_clients.each([&](const auto &id, const auto &client) { client->send(message); });
    }

    void Node::send(const packet_data_t &packet)
    {
        send(zmq_message_envelope_t(packet.serialize()));
    }

    void Node::send_keepalives()
    {
        while (m_running)
        {
            if (thread_sleep(m_stopping, Configuration::P2P::KEEPALIVE_INTERVAL))
            {
                break;
            }

            packet_keepalive_t packet(m_peer_db->peer_id());

            send(packet);

            // send via server to poke the clients
            reply(packet);
        }
    }

    void Node::send_peer_exchanges()
    {
        while (m_running)
        {
            if (thread_sleep(m_stopping, Configuration::P2P::PEER_EXCHANGE_INTERVAL))
            {
                break;
            }

            packet_peer_exchange_t packet(m_peer_db->peer_id(), m_server->port(), m_network_id);

            send(packet);
        }
    }

    Error Node::start(const std::vector<std::string> &seed_nodes)
    {
        if (!m_running)
        {
            {
                auto error = m_server->bind();

                if (error)
                {
                    return error;
                }
            }

            m_running = true;

            m_poller_thread = std::thread(&Node::poller, this);

            bool connected_to_seed = false;

            // attempt connections to the compiled in seed nodes
            for (const auto &seed_node : Configuration::P2P::SEED_NODES)
            {
                auto error = connect(seed_node.host, seed_node.port);

                if (!error && error != P2P_DUPE_CONNECT)
                {
                    connected_to_seed = true;
                }
            }

            // attempt connections to any alternate seed nodes specified
            for (const auto &seed_node : seed_nodes)
            {
                const auto seed = ip_address_t(seed_node);

                auto error =
                    connect(seed.to_string(), (seed.port() != 0) ? seed.port() : Configuration::P2P::DEFAULT_BIND_PORT);

                if (!error && error != P2P_DUPE_CONNECT)
                {
                    connected_to_seed = true;
                }
            }

            /**
             * If we cannot connect to ANY seed node and our peer list database is empty
             * then we need to fail out as we cannot connect to the peer to peer network
             */
            if (!m_seed_mode && !connected_to_seed && m_peer_db->count() == 0)
            {
                m_running = false;

                m_stopping.notify_all();

                if (m_poller_thread.joinable())
                {
                    m_poller_thread.join();
                }

                return MAKE_ERROR_MSG(P2P_SEED_CONNECT, "Could not connect to any seed nodes.");
            }

            m_keepalive_thread = std::thread(&Node::send_keepalives, this);

            m_peer_exchange_thread = std::thread(&Node::send_peer_exchanges, this);

            m_connection_manager_thread = std::thread(&Node::connection_manager, this);
        }

        return MAKE_ERROR(SUCCESS);
    }
} // namespace P2P
