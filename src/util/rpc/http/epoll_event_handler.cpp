// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "epoll_event_handler.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>

namespace cbdc::rpc {
    epoll_event_handler::~epoll_event_handler() {
        close(m_epoll);
    }

    auto epoll_event_handler::init() -> bool {
        return (m_epoll = epoll_create1(0)) != -1;
    }

    void epoll_event_handler::set_timeout(long timeout_ms) {
        if(timeout_ms == -1) {
            m_timeout_enabled = false;
            m_timeout_ms = 1000;
            return;
        }
        m_timeout_enabled = true;
        m_timeout_ms = timeout_ms;
    }

    void epoll_event_handler::register_fd(int fd, event_type et) {
        int retval{};
        if(et == event_type::remove) {
            retval = epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, nullptr);
            m_tracked.erase(fd);
        } else {
            epoll_event ev{};
            ev.data.fd = fd;
            ev.events = EPOLLET;
            if(et == event_type::in || et == event_type::inout) {
                ev.events |= EPOLLIN;
            }
            if(et == event_type::out || et == event_type::inout) {
                ev.events |= EPOLLOUT;
            }
            auto it = m_tracked.find(fd);
            if(it == m_tracked.end()) {
                retval = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev);
                m_tracked.insert(fd);
            } else {
                retval = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev);
            }
        }

        if(retval == -1) {
            perror("epoll_ctl");
        }
    }

    auto epoll_event_handler::poll() -> std::optional<std::vector<event>> {
        constexpr auto n_events = 1024;
        auto evs = std::array<struct epoll_event, n_events>();
        auto start_time = std::chrono::high_resolution_clock::now();
        auto event_count = epoll_wait(m_epoll,
                                      evs.data(),
                                      n_events,
                                      static_cast<int>(m_timeout_ms));
        if(event_count == -1) {
            perror("epoll_wait");
            return std::nullopt;
        }

        auto ret = std::vector<event>();

        if(m_timeout_enabled) {
            auto end_time = std::chrono::high_resolution_clock::now();
            if(end_time - start_time > std::chrono::milliseconds(m_timeout_ms)
               && event_count == 0) {
                ret.emplace_back(0, true);
                m_timeout_enabled = false;
                m_timeout_ms = 1000;
            }
        }

        for(size_t i = 0; i < static_cast<size_t>(event_count); i++) {
            auto& ev = evs[i];
            auto fd = ev.data.fd;
            ret.emplace_back(fd, false);
        }

        /* std::cout << (void*)this << "tracking: " << m_tracked.size() << "
         events: " << ret.size() << " timeout: " << m_timeout_enabled << " " <<
         m_timeout_ms << std::endl; */
        return ret;
    }
}
