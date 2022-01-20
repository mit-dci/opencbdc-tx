// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "socket.hpp"

#include <csignal>
#include <unistd.h>

namespace cbdc::network {
    socket::socket() {
        // Ignore SIGPIPE if the socket disconnects and we try to write to it
        static std::atomic_flag sigpipe_ignored = ATOMIC_FLAG_INIT;
        if(!sigpipe_ignored.test_and_set()) {
            // The definition of SIG_IGN contains a c-style cast and it's
            // implementation-defined in a standard library header so there's
            // nothing we can do about it.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
            std::signal(SIGPIPE, SIG_IGN);
        }
    }

    auto socket::get_addrinfo(const ip_address& address, port_number_t port)
        -> std::shared_ptr<addrinfo> {
        addrinfo hints{};
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* res0{};

        auto port_str = std::to_string(port);
        auto error
            = getaddrinfo(address.c_str(), port_str.c_str(), &hints, &res0);
        if(error != 0) {
            return nullptr;
        }

        auto ret = std::shared_ptr<addrinfo>(res0, [](addrinfo* p) {
            freeaddrinfo(p);
        });
        return ret;
    }

    auto socket::create_socket(int domain, int type, int protocol) -> bool {
        m_sock_fd = ::socket(domain, type, protocol);
        return m_sock_fd != -1;
    }

    auto socket::set_sockopts() -> bool {
        static constexpr int one = 1;
        if(setsockopt(m_sock_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))
           != 0) {
            ::close(m_sock_fd);
            m_sock_fd = -1;
            return false;
        }
        return true;
    }
}
