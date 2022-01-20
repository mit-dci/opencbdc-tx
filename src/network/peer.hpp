// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_NETWORK_PEER_H_
#define OPENCBDC_TX_SRC_NETWORK_PEER_H_

#include "common/blocking_queue.hpp"
#include "tcp_socket.hpp"

#include <atomic>
#include <thread>

namespace cbdc::network {
    /// \brief Maintains a TCP socket.
    ///
    /// Handles reconnecting to a TCP socket, queuing discrete packets to send,
    /// sending queued packets, and passing received packets to a callback
    /// function.
    class peer {
      public:
        /// Type for the packet receipt callback function. Accepts a pointer to
        /// a discrete packet received via the TCP socket.
        using callback_type
            = std::function<void(std::shared_ptr<cbdc::buffer>)>;

        /// \brief Constructor. Starts socket management threads.
        ///
        /// Starts a thread to send queued packets via the
        /// associated TCP socket. Starts a thread to receive packets and call
        /// a callback function. Starts a thread to monitor the connection
        /// status of the TCP socket and reconnect if the socket disconnects.
        /// \param sock TCP socket to manage.
        /// \param cb callback function to call with packets received by the socket.
        /// \param attempt_reconnect true if the instance should reconnect the TCP
        ///                          socket if it loses the connection.
        peer(std::unique_ptr<tcp_socket> sock,
             callback_type cb,
             bool attempt_reconnect);

        /// Destructor. Calls \ref shutdown().
        ~peer();

        peer(const peer&) = delete;
        auto operator=(const peer&) -> peer& = delete;

        peer(peer&&) = delete;
        auto operator=(peer&&) -> peer& = delete;

        /// \brief Sends buffered data.
        ///
        /// Queues a packet to send via the TCP socket. The recipient peer
        /// receives it as a discrete unit.
        /// \param data buffer to send.
        void send(const std::shared_ptr<cbdc::buffer>& data);

        /// Clears any packets in the pending send queue. Stops the send,
        /// receive, and reconnect threads. Disconnects the TCP socket.
        void shutdown();

        /// Indicates whether the TCP socket is currently connected.
        /// \return true if the TCP socket is connected.
        [[nodiscard]] auto connected() const -> bool;

      private:
        std::unique_ptr<tcp_socket> m_sock;

        blocking_queue<std::shared_ptr<cbdc::buffer>> m_send_queue;

        std::thread m_recv_thread;
        std::thread m_send_thread;

        std::thread m_reconnect_thread;
        std::mutex m_reconnect_mut;
        std::condition_variable m_reconnect_cv;
        bool m_reconnect{false};
        bool m_attempt_reconnect{};

        std::atomic_bool m_running{true};
        std::atomic_bool m_shut_down{false};

        callback_type m_recv_cb;

        void do_send();

        void do_recv();

        void do_reconnect();

        void close();

        void signal_reconnect();
    };
}

#endif
