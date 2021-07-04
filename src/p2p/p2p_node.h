// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_P2P_NETWORK_NODE_H
#define TURTLECOIN_P2P_NETWORK_NODE_H

#include "peer_database.h"

#include <condition_variable>
#include <tools/thread_safe_map.h>
#include <tools/thread_safe_set.h>
#include <zmq_client.h>
#include <zmq_server.h>

namespace P2P
{
    struct network_msg_t
    {
        network_msg_t(const crypto_hash_t &from, Types::Network::packet_data_t packet, bool is_server):
            from(from), packet(std::move(packet)), is_server(is_server)
        {
        }

        bool is_server = false;

        crypto_hash_t from;

        Types::Network::packet_data_t packet;
    };

    class Node
    {
      public:
        /**
         * Constructs a new instance of the Network Node object
         *
         * @param logger
         * @param path
         * @param bind_port
         * @param seed_mode
         * @param network_id
         */
        Node(
            logger &logger,
            const std::string &path,
            const uint16_t &bind_port,
            bool seed_mode = false,
            const crypto_hash_t &network_id = Configuration::P2P::NETWORK_ID);

        ~Node();

        /**
         * Returns the nodes external IP address if available
         *
         * @return
         */
        std::string external_address() const;

        /**
         * Returns a vector of the incoming connection addresses
         *
         * @return
         */
        std::vector<std::string> incoming_connected() const;

        /**
         * Returns the number of incoming connections
         *
         * @return
         */
        size_t incoming_connections() const;

        /**
         * Returns the current queue of network messages to be processed
         *
         * @return
         */
        ThreadSafeQueue<network_msg_t> &messages();

        /**
         * Returns a vector of the outgoing connection addresses
         *
         * @return
         */
        std::vector<std::string> outgoing_connected() const;

        /**
         * Returns the number of outgoing connections
         *
         * @return
         */
        size_t outgoing_connections() const;

        /**
         * Returns our peer ID
         *
         * @return
         */
        crypto_hash_t peer_id() const;

        /**
         * Returns the instance of the peer database
         *
         * @return
         */
        std::shared_ptr<PeerDB> peers() const;

        /**
         * Returns the nodes bind port
         *
         * @return
         */
        uint16_t port() const;

        /**
         * Replies via the server to a request by a client
         *
         * NOTE: message must be properly routed via the TO field
         *
         * @param message
         */
        void reply(const zmq_message_envelope_t &message);

        /**
         * Replies via the server to the specified client with the data packet specified
         *
         * @param to
         * @param packet
         */
        void reply(const crypto_hash_t &to, const packet_data_t &packet);

        /**
         * Returns if the P2P network node is running
         *
         * @return
         */
        bool running() const;

        /**
         * Sends the message out to all of the connected network peers
         *
         * @param message
         */
        void send(const zmq_message_envelope_t &message);

        /**
         * Sends the given data package out to all of the connected network peers
         *
         * @param packet
         */
        void send(const packet_data_t &packet);

        /**
         * Starts the P2P network node
         *
         * @param seed_nodes
         * @return
         */
        Error start(const std::vector<std::string> &seed_nodes = {});

      private:
        /**
         * Builds a handshake packet
         *
         * @return
         */
        packet_handshake_t build_handshake() const;

        /**
         * Builds the network peer list for exchange/handshake
         *
         * @return
         */
        std::vector<network_peer_t> build_peer_list() const;

        /**
         * Connects a new client instance to a server
         *
         * @param host
         * @param port
         * @return
         */
        Error connect(const std::string &host, const uint16_t &port);

        /**
         * The connection manager thread
         */
        void connection_manager();

        /**
         * Handles all incoming messages from both the server and the clients
         *
         * @param message
         * @param is_server
         */
        void handle_incoming_message(const zmq_message_envelope_t &message, bool is_server);

        /**
         * Handles the handshake packet
         *
         * @param from
         * @param peer_address
         * @param packet
         * @param is_server
         */
        void handle_packet(
            const crypto_hash_t &from,
            const std::string &peer_address,
            const Types::Network::packet_handshake_t &packet,
            bool is_server = false);

        /**
         * Handles a peer exchange packet
         *
         * @param from
         * @param peer_address
         * @param packet
         * @param is_server
         */
        void handle_packet(
            const crypto_hash_t &from,
            const std::string &peer_address,
            const Types::Network::packet_peer_exchange_t &packet,
            bool is_server = false);

        /**
         * Handles a keepalive packet
         *
         * @param from
         * @param peer_address
         * @param packet
         * @param is_server
         */
        void handle_packet(
            const crypto_hash_t &from,
            const std::string &peer_address,
            const Types::Network::packet_keepalive_t &packet,
            bool is_server = false);

        /**
         * Handles a data packet
         *
         * @param from
         * @param peer_address
         * @param packet
         * @param is_server
         */
        void handle_packet(
            const crypto_hash_t &from,
            const std::string &peer_address,
            const Types::Network::packet_data_t &packet,
            bool is_server = false);

        /**
         * The thread that sends keepalive messages
         */
        void send_keepalives();

        /**
         * The thread that sends peer exchange messages
         */
        void send_peer_exchanges();

        /**
         * The incoming message poller thread
         */
        void poller();

        std::atomic<bool> m_running, m_seed_mode;

        std::shared_ptr<PeerDB> m_peer_db;

        std::shared_ptr<Networking::ZMQServer> m_server;

        ThreadSafeMap<crypto_hash_t, std::shared_ptr<Networking::ZMQClient>> m_clients;

        ThreadSafeSet<crypto_hash_t> m_completed_handshake;

        ThreadSafeQueue<network_msg_t> m_messages;

        std::thread m_poller_thread, m_keepalive_thread, m_peer_exchange_thread, m_connection_manager_thread;

        logger m_logger;

        std::condition_variable m_stopping;

        crypto_hash_t m_network_id;
    };
} // namespace P2P

#endif
