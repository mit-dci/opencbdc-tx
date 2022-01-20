// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tcp_listener.hpp"

#include <unistd.h>

namespace cbdc::network {
    auto tcp_listener::listen(const ip_address& local_address,
                              port_number_t local_port) -> bool {
        auto res0 = get_addrinfo(local_address, local_port);
        if(!res0) {
            return false;
        }

        for(auto* res = res0.get(); res != nullptr; res = res->ai_next) {
            if(!create_socket(res->ai_family,
                              res->ai_socktype,
                              res->ai_protocol)) {
                continue;
            }

            if(!set_sockopts()) {
                continue;
            }

            if(bind(m_sock_fd, res->ai_addr, res->ai_addrlen) != 0) {
                ::close(m_sock_fd);
                m_sock_fd = -1;
                continue;
            }

            static constexpr auto max_listen_queue = 5;
            if(::listen(m_sock_fd, max_listen_queue) != 0) {
                ::close(m_sock_fd);
                m_sock_fd = -1;
                continue;
            }

            break;
        }

        return m_sock_fd != -1;
    }

    auto tcp_listener::accept(tcp_socket& sock) -> bool {
        sockaddr cli_addr{};
        unsigned int cli_len = sizeof(cli_addr);
        sock.m_sock_fd = ::accept(m_sock_fd, &cli_addr, &cli_len);
        return sock.m_sock_fd != -1;
    }

    void tcp_listener::close() {
        if(m_sock_fd != -1) {
            shutdown(m_sock_fd, SHUT_RDWR);
            ::close(m_sock_fd);
            m_sock_fd = -1;
        }
    }

    tcp_listener::~tcp_listener() {
        close();
    }
}
