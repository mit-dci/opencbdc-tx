// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "server.hpp"

namespace cbdc::sentinel::rpc {
    async_server::async_server(
        async_interface* impl,
        std::unique_ptr<cbdc::rpc::async_server<request, response>> srv)
        : m_impl(impl),
          m_srv(std::move(srv)) {
        m_processing_thread = std::thread([this]() {
            process();
        });
        m_srv->register_handler_callback(
            [&](const request& req,
                async_interface::result_callback_type callback) {
                auto req_item = request_queue_t{req, callback};
                m_request_queue.push(req_item);
                return true;
            });
    }
    bool operator<(const request_queue_t& a, const request_queue_t& b) {
        // Prioritize validate requests over execute requests
        return (std::holds_alternative<validate_request>(a.m_req)
                && std::holds_alternative<execute_request>(b.m_req));
    }
    async_server::~async_server() {
        m_running = false;
        if(m_processing_thread.joinable()) {
            m_processing_thread.join();
        }
    }
    void async_server::process() {
        auto q = request_queue_t();
        while(m_running) {
            if(m_request_queue.pop(q)) {
                std::visit(overloaded{[&](execute_request e_req) {
                                          return m_impl->execute_transaction(
                                              std::move(e_req),
                                              q.m_cb);
                                      },
                                      [&](validate_request v_req) {
                                          return m_impl->validate_transaction(
                                              std::move(v_req),
                                              q.m_cb);
                                      }},
                           q.m_req);
            }
        }
    }
}
