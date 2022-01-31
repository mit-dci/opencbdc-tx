// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_NETWORK_TCP_SOCKET_H_
#define OPENCBDC_TX_SRC_NETWORK_TCP_SOCKET_H_

#include "socket.hpp"
#include "util/common/buffer.hpp"
#include "util/serialization/buffer_serializer.hpp"
#include "util/serialization/util.hpp"

#include <atomic>

namespace cbdc::network {
    /// \brief Wrapper for a TCP socket.
    ///
    /// Manages a raw UNIX TCP socket. Handles sending and receiving discrete
    /// packets by providing a protocol for determining packet boundaries.
    /// Sends the size of the packet before the packet data. When receiving,
    /// reads the packet size and returns a discrete packet once the expected
    /// size is read in full.
    class tcp_socket : public socket {
      public:
        /// Constructs an empty, unconnected TCP socket.
        tcp_socket() = default;
        ~tcp_socket() override;

        tcp_socket(const tcp_socket&) = delete;
        auto operator=(const tcp_socket&) -> tcp_socket& = delete;

        tcp_socket(tcp_socket&&) = delete;
        auto operator=(tcp_socket&&) -> tcp_socket& = delete;

        /// Attempts to connect to the given endpoint.
        /// \param ep the endpoint to which this socket should connect
        /// \return true if the socket connected to the endpoint successfully.
        auto connect(const endpoint_t& ep) -> bool;

        /// Attempts to connect to the given remote address/port
        /// combination.
        /// \param remote_address the IP address of the remote endpoint
        /// \param remote_port the port number of the remote endpoint
        /// \return true if the socket connected to the endpoint successfully.
        auto connect(const ip_address& remote_address,
                     port_number_t remote_port) -> bool;

        /// Sends the given packet to the remote host.
        /// \param pkt the packet to send.
        /// \return true if the packet was sent successfully.
        [[nodiscard]] auto send(const buffer& pkt) const -> bool;

        /// Serialize the data and transmit it in a packet to the remote host.
        /// \param data data to serialize and send.
        /// \return true if the packet was sent successfully.
        template<typename T>
        [[nodiscard]] auto send(const T& data) const -> bool {
            auto pkt = make_buffer(data);
            return send(pkt);
        }

        /// Attempts to receive a packet from the remote host
        /// this function will block until there is data ready to receive or an
        /// error occurs.
        /// \param pkt the packet to receive into
        /// \return true if a packet was received successfully.
        [[nodiscard]] auto receive(buffer& pkt) const -> bool;

        /// Closes the connection with the remote host and unblocks
        /// any blocking calls to this socket.
        void disconnect();

        /// Reconnects to the previously connected endpoint. If connect was
        /// never called before, returns an error. Disconnects any previous
        /// endpoint before re-connecting.
        /// \return true if the socket reconnected successfully.
        auto reconnect() -> bool;

        /// Returns whether the socket successfully connected to an
        /// endpoint.
        /// \return true if connect() call succeeded. False if connect() has not
        ///         yet been called, the connect() call failed, or following a
        ///         disconnect() call.
        [[nodiscard]] auto connected() const -> bool;

      private:
        std::optional<ip_address> m_addr{};
        port_number_t m_port{};
        std::atomic_bool m_connected{false};
    };
}

#endif
