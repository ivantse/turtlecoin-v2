// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_NETWORKING_ZMQ_SHARED_H
#define TURTLECOIN_NETWORKING_ZMQ_SHARED_H

#include <atomic>
#include <condition_variable>
#include <crypto_common.h>
#include <errors.h>
#include <hashing.h>
#include <iostream>
#include <logger.h>
#include <mutex>
#include <set>
#include <thread>
#include <tools/thread_safe_set.h>
#include <zmq.hpp>

namespace Networking
{
    [[nodiscard]] std::tuple<Error, std::string, std::string> zmq_generate_keypair();

    [[nodiscard]] std::tuple<Error, std::string> zmq_generate_public_key(const std::string &secret_key);

    class zmq_connection_monitor : public zmq::monitor_t
    {
      public:
        zmq_connection_monitor();

        ~zmq_connection_monitor();

        void on_event_connected(const zmq_event_t &event, const char *addr) override;

        void on_event_connect_delayed(const zmq_event_t &event, const char *addr) override;

        void on_event_connect_retried(const zmq_event_t &event, const char *addr) override;

        void on_event_listening(const zmq_event_t &event, const char *addr) override;

        void on_event_accepted(const zmq_event_t &event, const char *addr) override;

        void on_event_closed(const zmq_event_t &event, const char *addr) override;

        void on_event_disconnected(const zmq_event_t &event, const char *addr) override;

        void on_event_handshake_succeeded(const zmq_event_t &event, const char *addr) override;

        void on_event_handshake_failed_auth(const zmq_event_t &event, const char *addr) override;

        void on_event_handshake_failed_protocol(const zmq_event_t &event, const char *addr) override;

        void on_event_handshake_failed_no_detail(const zmq_event_t &event, const char *addr) override;

        /**
         * Returns the connections (outgoing or incoming) currently associated with the socket
         *
         * @return
         */
        std::shared_ptr<ThreadSafeSet<std::string>> connected() const;

        /**
         * Returns the outgoing connections that are currently in a delayed state
         *
         * @return
         */
        std::shared_ptr<ThreadSafeSet<std::string>> delayed() const;

        /**
         * Returns whether the socket is in a listening state
         *
         * @return
         */
        bool listening() const;

        /**
         * Returns the outgoing connections that are currently being retried by the socket
         *
         * @return
         */
        std::shared_ptr<ThreadSafeSet<std::string>> retried() const;

        /**
         * Returns whether the monitor is currently running
         *
         * @return
         */
        bool running() const;

        /**
         * Starts the monitor on the given socket
         *
         * @param socket
         * @param addr
         * @param events
         */
        void start(zmq::socket_t &socket, int events = ZMQ_EVENT_ALL);

        std::condition_variable cv_connected;

      private:
        void listener();

        std::atomic<bool> m_running = false, m_listening = false;

        std::shared_ptr<ThreadSafeSet<std::string>> m_connected_peers, m_delayed_peers, m_retried_peers;

        std::thread m_poller;
    };
} // namespace Networking

#endif
