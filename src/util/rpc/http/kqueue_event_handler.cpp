// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "kqueue_event_handler.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <unistd.h>

namespace cbdc::rpc {
    kqueue_event_handler::~kqueue_event_handler() {
        close(m_kq);
    }

    auto kqueue_event_handler::init() -> bool {
        m_kq = kqueue();
        return m_kq != -1;
    }

    void kqueue_event_handler::set_timeout(long timeout_ms) {
        if(timeout_ms == -1) {
            m_timeout_enabled = false;
            m_timeout_ms = 1000;
            return;
        }
        m_timeout_enabled = true;
        m_timeout_ms = timeout_ms;
    }

    void kqueue_event_handler::register_fd(int fd, event_type et) {
        switch(et) {
            case event_type::remove: {
                struct kevent ev {};
                EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
                m_evs.emplace_back(std::move(ev));
            }
                {
                    struct kevent ev {};
                    EV_SET(&ev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
                    m_evs.emplace_back(std::move(ev));
                }
                break;
            case event_type::inout: {
                struct kevent ev {};
                EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, 0);
                m_evs.emplace_back(std::move(ev));
            }
                {
                    struct kevent ev {};
                    EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD, 0, 0, 0);
                    m_evs.emplace_back(std::move(ev));
                }
                break;
            case event_type::in: {
                struct kevent ev {};
                EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, 0);
                m_evs.emplace_back(std::move(ev));
            } break;
            case event_type::out: {
                struct kevent ev {};
                EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD, 0, 0, 0);
                m_evs.emplace_back(std::move(ev));
            } break;
        }
    }

    auto kqueue_event_handler::poll() -> std::optional<std::vector<event>> {
        auto timeout = timespec{};
        timeout.tv_sec = m_timeout_ms / 1000;
        timeout.tv_nsec = (m_timeout_ms % 1000) * 1000000;
        auto sz = std::max(1UL, m_evs.size());
        auto evs = std::vector<struct kevent>(sz);
        auto start_time = std::chrono::high_resolution_clock::now();
        auto nev = kevent(m_kq,
                          m_evs.data(),
                          static_cast<int>(m_evs.size()),
                          evs.data(),
                          static_cast<int>(evs.size()),
                          &timeout);
        m_evs.clear();
        if(nev == -1) {
            perror("kevent");
            return std::nullopt;
        }

        auto ret = std::vector<event>();

        if(m_timeout_enabled) {
            auto end_time = std::chrono::high_resolution_clock::now();
            if(end_time - start_time > std::chrono::milliseconds(m_timeout_ms)
               && nev == 0) {
                ret.emplace_back(0, true);
                m_timeout_enabled = false;
                m_timeout_ms = 1000;
            }
        }

        for(size_t i = 0; i < static_cast<size_t>(nev); i++) {
            auto& ev = evs[i];
            if(ev.flags & EV_ERROR) {
                continue;
            }
            ret.emplace_back(ev.ident, false);
        }

        return ret;
    }
}
