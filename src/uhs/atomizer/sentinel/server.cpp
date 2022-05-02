// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "server.hpp"

namespace cbdc::sentinel::rpc {
    server::server(
        interface* impl,
        std::unique_ptr<cbdc::rpc::async_server<request, response>> srv)
        : m_impl(impl),
          m_srv(std::move(srv)) {
        m_srv->register_handler_callback(
            [&](request req, callback_type callback) -> bool {
                m_queue.push({std::move(req), std::move(callback)});
                return true;
            });

        auto n_threads = std::thread::hardware_concurrency();
        for(size_t i = 0; i < n_threads; i++) {
            m_threads.emplace_back([&]() {
                while(handle_request()) {}
            });
        }
    }

    server::~server() {
        m_queue.clear();
        for(auto& t : m_threads) {
            if(t.joinable()) {
                t.join();
            }
        }
    }

    auto server::handle_request() -> bool {
        auto req = request_type();
        auto popped = m_queue.pop(req);
        if(!popped) {
            return false;
        }

        auto res = std::visit(
            overloaded{
                [&](execute_request e_req) -> std::optional<response> {
                    return m_impl->execute_transaction(std::move(e_req));
                },
                [&](validate_request v_req) -> std::optional<response> {
                    return m_impl->validate_transaction(std::move(v_req));
                }},
            req.first);

        req.second(res);

        return true;
    }
}
