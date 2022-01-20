// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_NETWORK_SOCKET_H_
#define OPENCBDC_TX_SRC_NETWORK_SOCKET_H_

#include <memory>
#include <netdb.h>
#include <string>

namespace cbdc::network {
    /// An IP addresses.
    using ip_address = std::string;
    /// Port number.
    using port_number_t = unsigned short;
    /// [host name, port number].
    using endpoint_t = std::pair<ip_address, port_number_t>;

    /// IP address for localhost.
    static const auto localhost = ip_address("127.0.0.1");

    /// \brief Generic superclass for network sockets.
    ///
    /// Provides a socket file descriptor and utility methods for configuring
    /// UNIX network sockets. Implementations must derive from this class; it
    /// cannot be used directly.
    /// \see tcp_socket.
    class socket {
      public:
        socket(const socket&) = delete;
        auto operator=(const socket&) -> socket& = delete;

        socket(socket&&) = delete;
        auto operator=(socket&&) -> socket& = delete;

      private:
        socket();
        virtual ~socket() = default;

        int m_sock_fd{-1};

        friend class tcp_socket;
        friend class tcp_listener;
        friend class socket_selector;

        static auto get_addrinfo(const ip_address& address, port_number_t port)
            -> std::shared_ptr<addrinfo>;

        virtual auto create_socket(int domain, int type, int protocol) -> bool;
        virtual auto set_sockopts() -> bool;
    };
}

#endif
