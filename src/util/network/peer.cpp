// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "peer.hpp"

#include <cassert>
#include <utility>

namespace cbdc::network {
    peer::peer(std::unique_ptr<tcp_socket> sock,
               peer::callback_type cb,
               bool attempt_reconnect)
        : m_sock(std::move(sock)),
          m_attempt_reconnect(attempt_reconnect),
          m_recv_cb(std::move(cb)) {
        do_send();
        do_recv();
        do_reconnect();
    }

    peer::~peer() {
        shutdown();
    }

    void peer::send(const std::shared_ptr<cbdc::buffer>& data) {
        if(!m_shut_down) {
            m_send_queue.push(data);
        }
    }

    void peer::shutdown() {
        m_shut_down = true;
        m_reconnect_cv.notify_one();
        if(m_reconnect_thread.joinable()) {
            m_reconnect_thread.join();
        }
        close();
    }

    auto peer::connected() const -> bool {
        return !m_shut_down && m_running && m_sock->connected();
    }

    void peer::do_send() {
        m_send_thread = std::thread([&]() {
            while(m_running) {
                std::shared_ptr<cbdc::buffer> pkt;
                if(!m_send_queue.pop(pkt)) {
                    assert(!m_running);
                    break;
                }

                if(pkt) {
                    const auto result = m_sock->send(*pkt);
                    if(!result) {
                        signal_reconnect();
                        return;
                    }
                }
            }
        });
    }

    void peer::do_recv() {
        m_recv_thread = std::thread([&]() {
            while(m_running) {
                auto pkt = std::make_shared<cbdc::buffer>();
                if(!m_sock->receive(*pkt)) {
                    signal_reconnect();
                    return;
                }

                m_recv_cb(std::move(pkt));
            }
        });
    }

    void peer::do_reconnect() {
        m_reconnect_thread = std::thread([&]() {
            while(!m_shut_down) {
                {
                    std::unique_lock<std::mutex> l(m_reconnect_mut);
                    m_reconnect_cv.wait(l, [&]() {
                        return m_reconnect || m_shut_down;
                    });
                    m_reconnect = false;
                }
                if(m_shut_down) {
                    break;
                }
                if(m_attempt_reconnect) {
                    close();
                    while(!m_shut_down && !m_sock->reconnect()) {
                        static constexpr auto retry_delay
                            = std::chrono::seconds(3);
                        std::unique_lock<std::mutex> l(m_reconnect_mut);
                        m_reconnect_cv.wait_for(l, retry_delay, [&]() -> bool {
                            return m_shut_down;
                        });
                    }
                    if(!m_shut_down) {
                        m_running = true;
                        do_send();
                        do_recv();
                    }
                } else {
                    m_shut_down = true;
                    close();
                    m_send_queue.clear();
                    return;
                }
            }
        });
    }

    void peer::close() {
        m_running = false;
        m_sock->disconnect();
        m_send_queue.clear();
        if(m_send_thread.joinable()) {
            m_send_thread.join();
        }
        if(m_recv_thread.joinable()) {
            m_recv_thread.join();
        }
        m_send_queue.reset();
    }

    void peer::signal_reconnect() {
        {
            std::lock_guard<std::mutex> l(m_reconnect_mut);
            m_reconnect = true;
        }
        m_reconnect_cv.notify_one();
    }
}
