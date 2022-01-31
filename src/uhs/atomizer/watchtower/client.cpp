// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "client.hpp"

#include "util/serialization/util.hpp"
#include "watchtower.hpp"

#include <utility>

namespace cbdc::watchtower {
    blocking_client::blocking_client(network::endpoint_t ep)
        : m_ep(std::move(ep)) {}

    blocking_client::~blocking_client() {
        m_network.close();
        m_client_thread.join();
    }

    auto blocking_client::init() -> bool {
        // TODO: Add error handling.
        auto ct = m_network.start_cluster_handler(
            {m_ep},
            [&](cbdc::network::message_t&& pkt)
                -> std::optional<cbdc::buffer> {
                auto deser = cbdc::buffer_serializer(*pkt.m_pkt);
                auto res = response(deser);
                m_res_q.push(
                    std::make_shared<cbdc::watchtower::response>(res));
                return std::nullopt;
            });

        if(!ct.has_value()) {
            return false;
        }

        m_client_thread = std::move(ct.value());

        return true;
    }

    auto blocking_client::request_best_block_height()
        -> std::shared_ptr<best_block_height_response> {
        auto data = request{cbdc::watchtower::best_block_height_request{}};
        auto pkt = make_shared_buffer(data);
        m_network.broadcast(pkt);

        std::shared_ptr<response> res;
        if(!m_res_q.pop(res)) {
            return nullptr;
        }
        return std::make_shared<best_block_height_response>(
            std::get<best_block_height_response>(res->payload()));
    }

    auto
    blocking_client::request_status_update(const status_update_request& req)
        -> std::shared_ptr<status_request_check_success> {
        auto data = request{req};
        auto pkt = make_shared_buffer(data);
        m_network.broadcast(pkt);

        std::shared_ptr<response> res;
        if(!m_res_q.pop(res)) {
            return nullptr;
        }
        return std::make_shared<status_request_check_success>(
            std::get<status_request_check_success>(res->payload()));
    }

    async_client::async_client(network::endpoint_t ep) : m_ep(std::move(ep)) {}

    async_client::~async_client() {
        m_handler_running = false;
        m_network.close();

        if(m_handler_thread.joinable()) {
            m_handler_thread.join();
        }
        m_client_thread.join();
    }

    auto async_client::init() -> bool {
        // TODO: Add error handling.
        auto ct = m_network.start_cluster_handler(
            {m_ep},
            [&](cbdc::network::message_t&& pkt)
                -> std::optional<cbdc::buffer> {
                auto deser = cbdc::buffer_serializer(*pkt.m_pkt);
                auto res = response(deser);
                m_res_q.push(
                    std::make_shared<cbdc::watchtower::response>(res));
                return std::nullopt;
            });

        if(!ct.has_value()) {
            return false;
        }

        m_client_thread = std::move(ct.value());

        m_handler_running = true;
        m_handler_thread = std::thread{[this]() {
            while(m_handler_running) {
                std::shared_ptr<response> res;
                if(!m_res_q.pop(res)) {
                    break;
                }

                std::visit(
                    overloaded{
                        [&](const status_request_check_success& s) {
                            m_su_handler(
                                std::make_shared<status_request_check_success>(
                                    s));
                        },
                        [&](const best_block_height_response& s) {
                            m_bbh_handler(
                                std::make_shared<best_block_height_response>(
                                    s));
                        }},
                    res->payload());
            }
        }};
        return true;
    }

    void async_client::request_best_block_height() {
        auto data = request{cbdc::watchtower::best_block_height_request{}};
        auto pkt = make_shared_buffer(data);
        m_network.broadcast(pkt);
    }

    void
    async_client::request_status_update(const status_update_request& req) {
        auto data = request{req};
        auto pkt = make_shared_buffer(data);
        m_network.broadcast(pkt);
    }

    void async_client::set_status_update_handler(
        const async_client::status_update_response_handler_t& handler) {
        m_su_handler = handler;
    }

    void async_client::set_block_height_handler(
        const async_client::best_block_height_handler_t& handler) {
        m_bbh_handler = handler;
    }
}
