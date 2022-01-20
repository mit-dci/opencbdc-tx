// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "connection_manager.hpp"

namespace cbdc::network {
    connection_manager::~connection_manager() {
        close();
    }

    auto connection_manager::listen(const ip_address& host,
                                    unsigned short port) -> bool {
        if(!m_listener.listen(host, port)) {
            return false;
        }
        if(!m_listen_selector.init()) {
            return false;
        }
        return m_listen_selector.add(m_listener);
    }

    auto connection_manager::pump() -> bool {
        while(m_running) {
            if(!m_listen_selector.wait()) {
                continue;
            }
            auto sock = std::make_unique<tcp_socket>();
            if(m_listener.accept(*sock)) {
                add(std::move(sock), false);
            } else {
                return false;
            }
        }

        return true;
    }

    void connection_manager::broadcast(const std::shared_ptr<buffer>& data) {
        {
            std::shared_lock<std::shared_mutex> l(m_peer_mutex);
            for(const auto& peer : m_peers) {
                peer.m_peer->send(data);
            }
        }
    }

    auto connection_manager::handle_messages() -> std::vector<message_t> {
        std::vector<message_t> pkts;

        {
            std::unique_lock<std::mutex> l(m_async_recv_mut);
            m_async_recv_cv.wait(l, [&]() {
                if(!m_running) {
                    return true;
                }

                return m_async_recv_data;
            });
            m_async_recv_data = false;

            for(auto&& q : m_async_recv_queues) {
                if(!q.empty()) {
                    pkts.push_back(std::move(q.front()));
                    q.pop();

                    // The queue still has packets so don't block
                    // next call.
                    if(!q.empty()) {
                        m_async_recv_data = true;
                    }
                }
            }
        }

        return pkts;
    }

    auto connection_manager::add(std::unique_ptr<tcp_socket> sock,
                                 bool attempt_reconnect) -> peer_id_t {
        size_t q_idx{};
        const auto peer_id = m_next_peer_id++;
        {
            // Register the async recv callback with the peer
            std::lock_guard<std::mutex> l(m_async_recv_mut);
            m_async_recv_queues.emplace_back();
            q_idx = m_async_recv_queues.size() - 1;
        }
        auto recv_cb = [&, q_idx, peer_id](std::shared_ptr<buffer> pkt) {
            {
                std::lock_guard<std::mutex> ll(m_async_recv_mut);
                m_async_recv_queues[q_idx].emplace(
                    message_t{std::move(pkt), peer_id});
                m_async_recv_data = true;
            }
            m_async_recv_cv.notify_one();
        };

        {
            std::unique_lock<std::shared_mutex> l(m_peer_mutex);
            auto p = std::make_unique<peer>(std::move(sock),
                                            recv_cb,
                                            attempt_reconnect);
            if(m_running) {
                m_peers.emplace_back(std::move(p), peer_id);
            }
        }

        return peer_id;
    }

    auto connection_manager::cluster_connect(
        const std::vector<endpoint_t>& endpoints,
        bool error_fatal) -> bool {
        for(const auto& ep : endpoints) {
            auto ep_sock = std::make_unique<tcp_socket>();
            if(!ep_sock->connect(ep)) {
                if(error_fatal) {
                    return false;
                }
            }
            add(std::move(ep_sock));
        }
        return true;
    }

    auto connection_manager::start_cluster_handler(
        const std::vector<endpoint_t>& endpoints,
        const packet_handler_t& handler) -> std::optional<std::thread> {
        if(!cluster_connect(endpoints)) {
            return std::nullopt;
        }

        return start_handler(handler);
    }

    auto connection_manager::start_server(const endpoint_t& listen_endpoint,
                                          const packet_handler_t& handler)
        -> std::optional<std::thread> {
        if(!listen(listen_endpoint.first, listen_endpoint.second)) {
            return std::nullopt;
        }

        return std::thread{[this, handler]() {
            auto l_thread = start_server_listener();
            auto h_thread = start_handler(handler);
            h_thread.join();
            l_thread.join();
        }};
    }

    auto connection_manager::start_server_listener() -> std::thread {
        return std::thread{[this]() {
            if(!pump()) {
                m_running = false;
            }
        }};
    }

    auto connection_manager::start_handler(const packet_handler_t& handler)
        -> std::thread {
        return std::thread{[this, handler]() {
            while(m_running) {
                auto pkts = handle_messages();

                for(auto&& pkt : pkts) {
                    if(!pkt.m_pkt) {
                        continue;
                    }

                    auto pid = pkt.m_peer_id;
                    auto res = handler(std::move(pkt));

                    if(res.has_value()) {
                        send(std::make_shared<buffer>(std::move(res.value())),
                             pid);
                    }
                }
            }
        }};
    }

    void connection_manager::close() {
        m_running = false;
        m_listen_selector.unblock();
        m_listener.close();
        {
            std::shared_lock<std::shared_mutex> l(m_peer_mutex);
            for(auto&& peer : m_peers) {
                peer.m_peer->shutdown();
            }
        }
        {
            std::unique_lock<std::shared_mutex> l(m_peer_mutex);
            m_peers.clear();
        }
        m_async_recv_cv.notify_all();
    }

    void connection_manager::send(const std::shared_ptr<buffer>& data,
                                  peer_id_t peer_id) {
        std::shared_ptr<peer> peer;
        {
            std::shared_lock<std::shared_mutex> l(m_peer_mutex);
            for(const auto& p : m_peers) {
                if(p.m_peer_id == peer_id) {
                    peer = p.m_peer;
                    break;
                }
            }
        }

        if(peer) {
            peer->send(data);
        }
    }

    auto connection_manager::peer_count() -> size_t {
        std::shared_lock<std::shared_mutex> l(m_peer_mutex);
        return m_peers.size();
    }

    void connection_manager::reset() {
        close();
        assert(!m_running);
        m_running = true;
        m_next_peer_id = 0;
        {
            std::lock_guard<std::mutex> l(m_async_recv_mut);
            m_async_recv_queues.clear();
            m_async_recv_data = false;
        }
    }

    auto connection_manager::send_to_one(const std::shared_ptr<buffer>& data)
        -> bool {
        bool sent{false};
        {
            std::shared_lock<std::shared_mutex> l(m_peer_mutex);
            for(const auto& p : m_peers) {
                if(p.m_peer->connected()) {
                    p.m_peer->send(data);
                    sent = true;
                    break;
                }
            }
        }
        return sent;
    }

    connection_manager::m_peer_t::m_peer_t(std::unique_ptr<peer> peer,
                                           peer_id_t peer_id)
        : m_peer(std::move(peer)),
          m_peer_id(peer_id) {}

    auto connection_manager::connected(peer_id_t peer_id) -> bool {
        {
            std::shared_lock<std::shared_mutex> l(m_peer_mutex);
            for(const auto& p : m_peers) {
                if(p.m_peer_id == peer_id) {
                    return p.m_peer->connected();
                }
            }
        }
        return false;
    }

    auto connection_manager::connected_to_one() -> bool {
        {
            std::shared_lock<std::shared_mutex> l(m_peer_mutex);
            for(const auto& p : m_peers) {
                if(p.m_peer->connected()) {
                    return true;
                }
            }
        }
        return false;
    }
}
