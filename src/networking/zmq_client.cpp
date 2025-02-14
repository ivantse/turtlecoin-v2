// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "zmq_client.h"

#include <tools/thread_helper.h>
#include <utilities.h>
#include <zmq_addon.hpp>

namespace Networking
{
    ZMQClient::ZMQClient(logger &logger, int timeout):
        m_identity(Crypto::random_hash()), m_running(false), m_timeout(timeout), m_logger(logger)
    {
        const auto identity = zmq::buffer(m_identity.data(), m_identity.size());

        m_socket = zmq::socket_t(m_context, zmq::socket_type::dealer);

        m_monitor.start(m_socket, ZMQ_EVENT_ALL);

        {
            const auto [error, public_key] = zmq_generate_public_key(Configuration::ZMQ::SERVER_SECRET_KEY);

            if (error)
            {
                throw std::runtime_error(error.to_string());
            }

            m_socket.set(zmq::sockopt::curve_serverkey, public_key.c_str());
        }

        const auto [error, public_key, secret_key] = zmq_generate_keypair();

        if (error)
        {
            throw std::runtime_error(error.to_string());
        }

        m_socket.set(zmq::sockopt::curve_publickey, public_key);

        m_socket.set(zmq::sockopt::curve_secretkey, secret_key);

        m_socket.set(zmq::sockopt::connect_timeout, timeout);

        m_socket.set(zmq::sockopt::immediate, false);

        m_socket.set(zmq::sockopt::routing_id, identity);

        m_socket.set(zmq::sockopt::ipv6, true);

        m_socket.set(zmq::sockopt::linger, 0);

        m_socket.set(zmq::sockopt::probe_router, true);
    }

    ZMQClient::~ZMQClient()
    {
        m_logger->debug("Shutting down ZMQ Client...");

        m_running = false;

        m_stopping.notify_all();

        if (m_thread_outgoing.joinable())
        {
            m_thread_outgoing.join();

            m_logger->trace("Client outgoing thread shut down successfully");
        }

        if (m_thread_incoming.joinable())
        {
            m_thread_incoming.join();

            m_logger->trace("Client incoming thread shut down successfully");
        }

        std::scoped_lock lock(m_socket_mutex);

        m_socket.close();

        m_logger->debug("ZMQ Client shutdown complete");
    }

    Error ZMQClient::connect(const std::string &host, const uint16_t &port)
    {
        try
        {
            m_logger->debug("Attempting to connect ZMQ Client to {0}:{1}", host, port);

            std::unique_lock lock(m_connecting);

            std::scoped_lock socket_lock(m_socket_mutex);

            m_socket.connect("tcp://" + host + ":" + std::to_string(port));

            const auto timeout = m_monitor.cv_connected.wait_for(
                lock, std::chrono::milliseconds(Configuration::DEFAULT_CONNECTION_TIMEOUT));

            if (timeout == std::cv_status::timeout)
            {
                return MAKE_ERROR_MSG(ZMQ_CONNECT_ERROR, "Could not connect to " + host + ":" + std::to_string(port));
            }

            if (!m_running)
            {
                m_running = true;

                m_thread_incoming = std::thread(&ZMQClient::incoming_thread, this);

                m_thread_outgoing = std::thread(&ZMQClient::outgoing_thread, this);
            }

            m_logger->debug("Connected ZMQ Client to {0}:{1}", host, port);

            return MAKE_ERROR(SUCCESS);
        }
        catch (const zmq::error_t &e)
        {
            return MAKE_ERROR_MSG(ZMQ_CONNECT_ERROR, e.what());
        }
    }

    std::vector<std::string> ZMQClient::connected() const
    {
        std::vector<std::string> results;

        m_monitor.connected()->each([&](const std::string &elem) { results.emplace_back(elem); });

        return results;
    }

    crypto_hash_t ZMQClient::identity() const
    {
        return m_identity;
    }

    bool ZMQClient::is_connected() const
    {
        return !m_monitor.connected()->empty();
    }

    void ZMQClient::incoming_thread()
    {
        while (m_running)
        {
            try
            {
                std::scoped_lock lock(m_socket_mutex);

                zmq::multipart_t messages(m_socket, ZMQ_DONTWAIT);

                // we expect exactly one message part and it should not be empty
                if (messages.size() == 1 && !messages.front().empty())
                {
                    auto message = messages.pop();

                    const auto ident_data = ZMQ_GETS(message, "Identity");

                    const auto from = ZMQ_IDENT_TO_HASH(ident_data);

                    const auto data = ZMQ_MSG_TO_VECTOR(message);

                    auto routable_msg = zmq_message_envelope_t(m_identity, from, data);

                    {
                        const auto unsafe_host = ZMQ_GETS(message, "Peer-Address");

                        const auto [host, port, hash] = Utilities::normalize_host_port(unsafe_host);

                        routable_msg.peer_address = host;
                    }

                    m_incoming_msgs.push(routable_msg);

                    m_logger->trace(
                        "Message received from {0}: {1}",
                        routable_msg.peer_address,
                        Crypto::StringTools::to_hex(routable_msg.payload.data(), routable_msg.payload.size()));
                }
            }
            catch (const zmq::error_t &e)
            {
                m_logger->trace("Could not read incoming ZMQ message: {0}", e.what());
            }

            if (thread_sleep(m_stopping))
            {
                break;
            }
        }
    }

    ThreadSafeQueue<zmq_message_envelope_t> &ZMQClient::messages()
    {
        return m_incoming_msgs;
    }

    void ZMQClient::outgoing_thread()
    {
        while (m_running)
        {
            while (!m_outgoing_msgs.empty())
            {
                auto message = m_outgoing_msgs.pop();

                // skip empty messages
                if (message.payload.empty())
                {
                    continue;
                }

                try
                {
                    std::scoped_lock lock(m_socket_mutex);

                    m_socket.send(message.payload_msg(), zmq::send_flags::dontwait);

                    m_logger->trace(
                        "Message sent to {0}: {1}",
                        message.to.to_string(),
                        Crypto::StringTools::to_hex(message.payload.data(), message.payload.size()));
                }
                catch (const zmq::error_t &e)
                {
                    m_logger->debug("Could not send ZMQ message: {0}", e.what());
                }
            }

            if (thread_sleep(m_stopping))
            {
                break;
            }
        }
    }

    bool ZMQClient::running() const
    {
        return m_running;
    }

    void ZMQClient::send(const zmq_message_envelope_t &message)
    {
        if (!message.payload.empty() && m_running)
        {
            m_outgoing_msgs.push(message);
        }
    }
} // namespace Networking
