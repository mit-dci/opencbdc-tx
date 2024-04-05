// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tcp_socket.hpp"

#include <array>
#include <cstring>
#include <unistd.h>
#include <netinet/tcp.h> // for TCP_NODELAY, TCP_QUICKACK

namespace cbdc::network {
    auto tcp_socket::connect(const endpoint_t& ep) -> bool {
        return connect(ep.first, ep.second);
    }

    auto tcp_socket::connect(const ip_address& remote_address,
                             port_number_t remote_port) -> bool {
        m_addr = remote_address;
        m_port = remote_port;
        auto res0 = get_addrinfo(remote_address, remote_port);
        if(!res0) {
            return false;
        }

        for(auto* res = res0.get(); res != nullptr; res = res->ai_next) {
            if(!create_socket(res->ai_family,
                              res->ai_socktype,
                              res->ai_protocol)) {
                continue;
            }

            if(::connect(m_sock_fd, res->ai_addr, res->ai_addrlen) != 0) {
                ::close(m_sock_fd);
                m_sock_fd = -1;
                continue;
            }

            static constexpr int one = 1;
            setsockopt(m_sock_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)); // sending side. needed?
            setsockopt(m_sock_fd, IPPROTO_TCP, TCP_QUICKACK, &one, sizeof(one)); // receiving side

            break;
        }

        m_connected = m_sock_fd != -1;

        return m_connected;
    }

    void tcp_socket::disconnect() {
        if(m_sock_fd != -1) {
            m_connected = false;
            shutdown(m_sock_fd, SHUT_RDWR);
            close(m_sock_fd);
            m_sock_fd = -1;
        }
    }

    tcp_socket::~tcp_socket() {
        disconnect();
    }

    auto tcp_socket::send(const buffer& pkt) const -> bool {
        const auto sz_val = static_cast<uint64_t>(pkt.size());
        std::array<std::byte, sizeof(sz_val)> sz_arr{};
        std::memcpy(sz_arr.data(), &sz_val, sizeof(sz_val));
        size_t total_written = 0;
        while(total_written != sizeof(sz_val)) {
            auto n = write(m_sock_fd,
                           &sz_arr.at(total_written),
                           sizeof(sz_val) - total_written);
            if(n <= 0) {
                return false;
            }
            total_written += static_cast<size_t>(n);
        }

        total_written = 0;
        while(total_written
              != static_cast<decltype(total_written)>(pkt.size())) {
            auto n = write(m_sock_fd,
                           pkt.data_at(total_written),
                           pkt.size() - total_written);
            if(n <= 0) {
                return false;
            }
            total_written += static_cast<size_t>(n);
        }

        return true;
    }

    auto tcp_socket::receive(buffer& pkt) const -> bool {
        static constexpr int one = 1;
        // apparently TCP_QUICKACK needs to be re-set after each read (incurring a syscall...)
        // cf. https://github.com/netty/netty/issues/13610

        uint64_t pkt_sz{};
        std::array<std::byte, sizeof(pkt_sz)> sz_buf{};
        uint64_t total_read{0};
        while(total_read != sz_buf.size()) {
            auto n = read(m_sock_fd,
                          &sz_buf.at(total_read),
                          sizeof(pkt_sz) - total_read);
            setsockopt(m_sock_fd, IPPROTO_TCP, TCP_QUICKACK, &one, sizeof(one)); // receiving side
            if(n <= 0) {
                return false;
            }
            total_read += static_cast<uint64_t>(n);
        }
        std::memcpy(&pkt_sz, sz_buf.data(), sizeof(pkt_sz));

        pkt.clear();

        total_read = 0;
        auto buf = std::vector<std::byte>(pkt_sz);
        while(total_read < pkt_sz) {
            const auto buf_sz = pkt_sz - total_read;
            auto n = read(m_sock_fd, buf.data(), buf_sz);
            setsockopt(m_sock_fd, IPPROTO_TCP, TCP_QUICKACK, &one, sizeof(one)); // receiving side
            if(n <= 0) {
                return false;
            }

            total_read += static_cast<uint64_t>(n);
            pkt.append(buf.data(), static_cast<size_t>(n));
        }

        return true;
    }

    auto tcp_socket::reconnect() -> bool {
        disconnect();
        if(!m_addr) {
            return false;
        }
        return connect(*m_addr, m_port);
    }

    auto tcp_socket::connected() const -> bool {
        return m_connected;
    }
}
