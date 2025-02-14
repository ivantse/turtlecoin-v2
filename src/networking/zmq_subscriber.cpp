// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "zmq_subscriber.h"

#include <tools/thread_helper.h>
#include <utilities.h>
#include <zmq_addon.hpp>

namespace Networking
{
    ZMQSubscriber::ZMQSubscriber(logger &logger, int timeout):
        m_identity(Crypto::random_hash()), m_running(false), m_timeout(timeout), m_logger(logger)
    {
        m_socket = zmq::socket_t(m_context, zmq::socket_type::sub);

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

        m_socket.set(zmq::sockopt::immediate, true);

        m_socket.set(zmq::sockopt::ipv6, true);

        m_socket.set(zmq::sockopt::linger, 0);
    }

    ZMQSubscriber::~ZMQSubscriber()
    {
        m_logger->debug("Shutting down ZMQ Subscriber...");

        m_running = false;

        m_stopping.notify_all();

        if (m_thread_incoming.joinable())
        {
            m_thread_incoming.join();

            m_logger->trace("ZMQ Subscriber incoming thread shut down successfully");
        }

        std::unique_lock lock(m_socket_mutex);

        m_socket.close();

        m_logger->debug("ZMQ Subscriber shutdown complete");
    }

    Error ZMQSubscriber::connect(const std::string &host, const uint16_t &port)
    {
        try
        {
            m_logger->debug("Attempting to connect ZMQ Subscriber to {0}:{1}", host, port);

            std::unique_lock lock(m_socket_mutex);

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

                m_thread_incoming = std::thread(&ZMQSubscriber::incoming_thread, this);
            }

            m_logger->debug("Connected ZMQ Subscriber to {0}:{1}", host, port);

            return MAKE_ERROR(SUCCESS);
        }
        catch (const zmq::error_t &e)
        {
            return MAKE_ERROR_MSG(ZMQ_CONNECT_ERROR, e.what());
        }
    }

    std::vector<std::string> ZMQSubscriber::connected() const
    {
        std::vector<std::string> results;

        m_monitor.connected()->each([&](const std::string &elem) { results.emplace_back(elem); });

        return results;
    }

    void ZMQSubscriber::disconnect(const std::string &host, const uint16_t &port)
    {
        try
        {
            std::unique_lock lock(m_socket_mutex);

            m_socket.disconnect("tcp://" + host + ":" + std::to_string(port));
        }
        catch (...)
        {
            // we don't care if a disconnect fails
        }
    }

    crypto_hash_t ZMQSubscriber::identity() const
    {
        return m_identity;
    }

    bool ZMQSubscriber::is_connected() const
    {
        return !m_monitor.connected()->empty();
    }

    void ZMQSubscriber::incoming_thread()
    {
        while (m_running)
        {
            try
            {
                std::unique_lock lock(m_socket_mutex);

                zmq::multipart_t messages(m_socket, ZMQ_DONTWAIT);

                // we expect exactly two message parts and the second part should not be empty
                if (messages.size() == 2 && !messages.back().empty())
                {
                    auto message = messages.pop();

                    auto data = ZMQ_MSG_TO_VECTOR(message);

                    const auto subject = crypto_hash_t(data);

                    message = messages.pop();

                    data = ZMQ_MSG_TO_VECTOR(message);

                    auto routable_msg = zmq_message_envelope_t(m_identity, data);

                    routable_msg.subject = subject;

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

    ThreadSafeQueue<zmq_message_envelope_t> &ZMQSubscriber::messages()
    {
        return m_incoming_msgs;
    }

    bool ZMQSubscriber::running() const
    {
        return m_running;
    }

    void ZMQSubscriber::subscribe(const crypto_hash_t &subject)
    {
        const auto buf = zmq::buffer(subject.data(), subject.size());

        m_socket.set(zmq::sockopt::subscribe, buf);

        m_logger->debug("ZMQ Subscriber subject added: {0}", subject.to_string());
    }

    void ZMQSubscriber::unsubscribe(const crypto_hash_t &subject)
    {
        const auto buf = zmq::buffer(subject.data(), subject.size());

        m_socket.set(zmq::sockopt::unsubscribe, buf);

        m_logger->debug("ZMQ Subscriber subject removed: {0}", subject.to_string());
    }
} // namespace Networking
