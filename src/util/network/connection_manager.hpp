// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_NETWORK_CONNECTION_MANAGER_H_
#define OPENCBDC_TX_SRC_NETWORK_CONNECTION_MANAGER_H_

#include "peer.hpp"
#include "socket_selector.hpp"
#include "tcp_listener.hpp"
#include "tcp_socket.hpp"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <random>
#include <shared_mutex>
#include <sys/socket.h>
#include <thread>

namespace cbdc::network {
    /// Peer IDs within a \ref connection_manager.
    using peer_id_t = size_t;

    /// \brief Received message type.
    ///
    /// Message type passed to packet handler callbacks. Also returned from
    /// blocking receive methods in \ref connection_manager.
    struct message_t {
        /// Packet data.
        std::shared_ptr<buffer> m_pkt;
        /// Peer ID that sent packet.
        peer_id_t m_peer_id{};
    };

    /// \brief Function type for packet handler callbacks.
    ///
    /// Receives a packet to handle. Optionally returns a packet to forward to
    /// the peer that sent the original packet, or std::nullopt to send nothing
    /// back to the peer.
    using packet_handler_t = std::function<std::optional<buffer>(message_t&&)>;

    /// \brief Manages a group of \ref peer s.
    ///
    /// Utility class for managing groups of peers. Handles listening for
    /// incoming connections on a TCP socket, connecting to outgoing peers,
    /// and passing incoming packets to a handler callback. Supports sending a
    /// packet to a specific peer, or broadcasting a packet to all peers.
    class connection_manager {
      public:
        connection_manager() = default;

        connection_manager(const connection_manager&) = delete;
        auto operator=(const connection_manager&)
            -> connection_manager& = delete;

        connection_manager(connection_manager&&) = delete;
        auto operator=(connection_manager&&) -> connection_manager& = delete;

        ~connection_manager();

        /// Starts a listener to listen for inbound connections on the
        /// specified IP address and port. \param host IP address to use.
        /// \param port port to use.
        /// \return true if creating the listener succeeded. False if port or IP is
        /// unavailable.
        [[nodiscard]] auto listen(const ip_address& host, unsigned short port)
            -> bool;

        /// Listens for and accepts inbound connections.
        /// \return true on a clean shutdown. False upon a socket accept failure.
        [[nodiscard]] auto pump() -> bool;

        /// Sends the provided data to all added peers.
        /// \param data packet to to send.
        /// \see connection_manager::add
        void broadcast(const std::shared_ptr<buffer>& data);

        /// Serialize the data and broadcast it to all peers. Wraps
        /// connection_manager::broadcast.
        /// \param data data to serialize and send.
        template<typename Ta>
        void broadcast(const Ta& data) {
            auto pkt = make_shared_buffer(data);
            return broadcast(pkt);
        }

        /// Collects and return unhandled packets received from connected
        /// peers. \return vector of packets to handle.
        /// \note returned packets may be empty; check before dereferencing.
        [[nodiscard]] auto handle_messages() -> std::vector<message_t>;

        /// Registers the provided socket as a peer to which messages can be
        /// sent or broadcast.
        /// \param sock a connected socket.
        /// \param attempt_reconnect true if the socket should automatically
        ///                          reconnect if disconnected.
        /// \return peer ID for this network.
        auto add(std::unique_ptr<tcp_socket> sock,
                 bool attempt_reconnect = true) -> peer_id_t;

        /// Establishes connections to the provided list of endpoints.
        /// \param endpoints set of server endpoints to which to establish TCP socket connections.
        /// \param error_fatal true if this function should abort and return false after a single failed connection attempt.
        /// \return false if any of the connections failed while the error_fatal flag is true.
        auto cluster_connect(const std::vector<endpoint_t>& endpoints,
                             bool error_fatal = true) -> bool;

        /// Connects to the provided endpoints and calls the provided handler
        /// for packets received from those endpoints.
        /// \param endpoints set of server endpoints to which to establish TCP socket connections.
        /// \param handler function to handle packets from server connections.
        /// \return the thread on which the handler will be called, or nullopt if any client connection fails. May be joined by callers.
        /// \note Calling this method and start_server on the same connection_manager
        ///       instance will result in two handler threads.
        [[nodiscard]] auto
        start_cluster_handler(const std::vector<endpoint_t>& endpoints,
                              const packet_handler_t& handler)
            -> std::optional<std::thread>;

        /// Establishes a server at the specified endpoint which handles
        /// inbound traffic with the specified handler function.
        /// \param listen_endpoint the endpoint at which to start the server.
        /// \param handler function to handle packets from client connections.
        /// \return the thread on which the handler will be called, or nullopt if the server fails to start. May be joined by callers.
        /// \note Calling this method and start_cluster_handler on the same
        ///       connection_manager instance will result in two handler
        ///       threads.
        [[nodiscard]] auto start_server(const endpoint_t& listen_endpoint,
                                        const packet_handler_t& handler)
            -> std::optional<std::thread>;

        /// Launches a thread that listens for and accepts inbound connections.
        /// Called after listen().
        /// \return listener thread.
        [[nodiscard]] auto start_server_listener() -> std::thread;

        /// Starts a thread to handle messages from established connections
        /// using the specified handler function.
        /// \param handler function to handle packets from client connections.
        /// \return the thread on which the handler will be called.
        [[nodiscard]] auto start_handler(const packet_handler_t& handler)
            -> std::thread;

        /// Shuts down the network listener and all existing peer connections.
        void close();

        /// Sends the provided data to the specified peer. Conducts an O(n)
        /// search for the target peer. \param data data packet to send.
        /// \param peer_id ID of the peer to whom to send data.
        void send(const std::shared_ptr<buffer>& data, peer_id_t peer_id);

        /// Serialize the data and transmit it in a packet to the remote host
        /// at the specified peer ID.
        /// \param data data to serialize and send.
        /// \param peer_id ID of the peer to whom to send data.
        template<typename Ta>
        void send(const Ta& data, peer_id_t peer_id) {
            auto pkt = make_shared_buffer(data);
            return send(pkt, peer_id);
        }

        /// Returns the number of peers connected to this network.
        /// \return number of peers connected to this network.
        [[nodiscard]] auto peer_count() -> size_t;

        /// Resets the network instance to a fresh state. Callers must close()
        /// the network and join() any handler threads before re-using the
        /// instance with this function.
        void reset();

        /// Send the provided data to an online peer managed by this network.
        /// \param data packet to send.
        /// \return flag to indicate whether the packet was sent to a peer.
        [[nodiscard]] auto send_to_one(const std::shared_ptr<buffer>& data)
            -> bool;

        /// Serialize and send the provided data to an online peer managed by
        /// this network. Wraps connection_manager::send_to_one.
        /// \param data serializable object to send.
        /// \return flag to indicate whether the packet was sent to a peer.
        template<typename T>
        [[nodiscard]] auto send_to_one(const T& data) -> bool {
            auto pkt = make_shared_buffer(data);
            return send_to_one(pkt);
        }

        /// Determines whether the given peer ID is connected
        /// \param peer_id the peer to check
        /// \return true if the peer is connected, otherwise false
        [[nodiscard]] auto connected(peer_id_t peer_id) -> bool;

        /// Determines if the network is connected to at least one peer.
        /// \return true if at least one peer is connected.
        [[nodiscard]] auto connected_to_one() -> bool;

      private:
        tcp_listener m_listener;

        struct m_peer_t {
            m_peer_t() = delete;

            m_peer_t(std::unique_ptr<peer> peer, peer_id_t peer_id);
            ~m_peer_t() = default;

            auto operator=(const m_peer_t& other) -> m_peer_t& = default;
            m_peer_t(const m_peer_t& other) = default;

            auto operator=(m_peer_t&& other) noexcept -> m_peer_t& = default;
            m_peer_t(m_peer_t&& other) noexcept = default;

            std::shared_ptr<peer> m_peer;
            peer_id_t m_peer_id;
        };

        std::vector<m_peer_t> m_peers;
        std::atomic<peer_id_t> m_next_peer_id{0};

        std::shared_mutex m_peer_mutex;

        std::atomic_bool m_running{true};

        std::mutex m_async_recv_mut;
        std::condition_variable m_async_recv_cv;
        std::vector<std::queue<message_t>> m_async_recv_queues;
        bool m_async_recv_data{false};

        socket_selector m_listen_selector;

        std::random_device m_r;
        std::default_random_engine m_rnd{m_r()};
    };
}

#endif // OPENCBDC_TX_SRC_NETWORK_CONNECTION_MANAGER_H_
