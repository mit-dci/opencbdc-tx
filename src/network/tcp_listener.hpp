// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_NETWORK_TCP_LISTENER_H_
#define OPENCBDC_TX_SRC_NETWORK_TCP_LISTENER_H_

#include "tcp_socket.hpp"

namespace cbdc::network {
    /// Listens for incoming TCP connections on a given endpoint.
    class tcp_listener : public socket {
      public:
        /// Constructs a new tcp_listener.
        tcp_listener() = default;
        ~tcp_listener() override;

        tcp_listener(const tcp_listener&) = delete;
        auto operator=(const tcp_listener&) -> tcp_listener& = delete;

        tcp_listener(tcp_listener&&) = delete;
        auto operator=(tcp_listener&&) -> tcp_listener& = delete;

        /// Starts the listener on the given local port and address.
        /// \param local_address the address of the interface to listen on
        /// \param local_port the port number to listen on
        /// \return true if the listener started listening successfully.
        auto listen(const ip_address& local_address, port_number_t local_port)
            -> bool;

        /// Blocks until an incoming connection is ready and
        /// populates the given socket.
        /// \param sock the socket to attach to the incoming connection
        /// \return true if the listener successfully accepted a connection.
        auto accept(tcp_socket& sock) -> bool;

        /// Stops the listener and unblocks any blocking calls associated
        /// with this listener.
        void close();
    };
}

#endif
