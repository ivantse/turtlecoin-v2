// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "zmq_shared.h"

#include <utilities.h>

namespace Networking
{
    std::tuple<Error, std::string, std::string> zmq_generate_keypair()
    {
        char public_key[41] = {0}, secret_key[41] = {0};

        if (zmq_curve_keypair(public_key, secret_key) != 0)
        {
            return {
                MAKE_ERROR_MSG(ZMQ_GENERIC_ERROR, "Could not generate ZMQ CURVE key pair"),
                std::string(),
                std::string()};
        }

        return {MAKE_ERROR(SUCCESS), public_key, secret_key};
    }

    std::tuple<Error, std::string> zmq_generate_public_key(const std::string &secret_key)
    {
        char public_key[41] = {0};

        if (zmq_curve_public(public_key, secret_key.c_str()) != 0)
        {
            return {
                MAKE_ERROR_MSG(ZMQ_GENERIC_ERROR, "Could not generate ZMQ CURVE public key from secret key"),
                std::string()};
        }

        return {MAKE_ERROR(SUCCESS), public_key};
    }

    zmq_connection_monitor::zmq_connection_monitor()
    {
        m_connected_peers = std::make_shared<ThreadSafeSet<std::string>>();

        m_delayed_peers = std::make_shared<ThreadSafeSet<std::string>>();

        m_retried_peers = std::make_shared<ThreadSafeSet<std::string>>();
    }

    zmq_connection_monitor::~zmq_connection_monitor()
    {
        m_running = false;

        if (m_poller.joinable())
        {
            m_poller.join();
        }
    }

    void zmq_connection_monitor::on_event_connected(const zmq_event_t &event, const char *addr)
    {
        auto [host, port, hash] = Utilities::normalize_host_port(addr);

        host = host + ":" + std::to_string(port);

        m_connected_peers->insert(host);

        m_delayed_peers->erase(host);

        m_retried_peers->erase(host);

        cv_connected.notify_all();
    }

    void zmq_connection_monitor::on_event_connect_delayed(const zmq_event_t &event, const char *addr)
    {
        auto [host, port, hash] = Utilities::normalize_host_port(addr);

        host = host + ":" + std::to_string(port);

        m_delayed_peers->insert(host);

        m_retried_peers->erase(host);

        m_connected_peers->erase(host);
    }

    void zmq_connection_monitor::on_event_connect_retried(const zmq_event_t &event, const char *addr)
    {
        auto [host, port, hash] = Utilities::normalize_host_port(addr);

        host = host + ":" + std::to_string(port);

        m_retried_peers->insert(host);

        m_delayed_peers->erase(host);

        m_connected_peers->erase(host);
    }

    void zmq_connection_monitor::on_event_listening(const zmq_event_t &event, const char *addr)
    {
        m_listening = true;
    }

    void zmq_connection_monitor::on_event_accepted(const zmq_event_t &event, const char *addr)
    {
        // do nothing
    }

    void zmq_connection_monitor::on_event_closed(const zmq_event_t &event, const char *addr)
    {
        auto [host, port, hash] = Utilities::normalize_host_port(addr);

        host = host + ":" + std::to_string(port);

        m_connected_peers->erase(host);
    }

    void zmq_connection_monitor::on_event_disconnected(const zmq_event_t &event, const char *addr)
    {
        auto [host, port, hash] = Utilities::normalize_host_port(addr);

        host = host + ":" + std::to_string(port);

        m_connected_peers->erase(host);
    }

    void zmq_connection_monitor::on_event_handshake_succeeded(const zmq_event_t &event, const char *addr)
    {
        // do nothing
    }

    void zmq_connection_monitor::on_event_handshake_failed_auth(const zmq_event_t &event, const char *addr)
    {
        // do nothing
    }

    void zmq_connection_monitor::on_event_handshake_failed_protocol(const zmq_event_t &event, const char *addr)
    {
        // do nothing
    }

    void zmq_connection_monitor::on_event_handshake_failed_no_detail(const zmq_event_t &event, const char *addr)
    {
        // do nothing
    }

    std::shared_ptr<ThreadSafeSet<std::string>> zmq_connection_monitor::connected() const
    {
        return m_connected_peers;
    }

    std::shared_ptr<ThreadSafeSet<std::string>> zmq_connection_monitor::delayed() const
    {
        return m_delayed_peers;
    }

    void zmq_connection_monitor::listener()
    {
        while (m_running)
        {
            this->check_event(100);
        }
    }

    bool zmq_connection_monitor::listening() const
    {
        return m_listening;
    }

    std::shared_ptr<ThreadSafeSet<std::string>> zmq_connection_monitor::retried() const
    {
        return m_retried_peers;
    }

    bool zmq_connection_monitor::running() const
    {
        return m_running;
    }

    void zmq_connection_monitor::start(zmq::socket_t &socket, int events)
    {
        if (!m_running)
        {
            const auto guid = Crypto::random_hash();

            init(socket, "inproc://monitor-" + guid.to_string(), events);

            m_running = true;

            m_poller = std::thread(&zmq_connection_monitor::listener, this);
        }
    }
} // namespace Networking
