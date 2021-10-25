// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "server.hpp"

namespace cbdc::sentinel::rpc {
    server::server(
        interface* impl,
        std::unique_ptr<cbdc::rpc::blocking_server<request, response>> srv)
        : m_impl(impl),
          m_srv(std::move(srv)) {
        m_srv->register_handler_callback([&](const request& req)
                                             -> std::optional<response> {
            auto res = std::visit(
                overloaded{
                    [&](execute_request e_req) -> std::optional<response> {
                        return m_impl->execute_transaction(std::move(e_req));
                    },
                    [&](validate_request v_req) -> std::optional<response> {
                        return m_impl->validate_transaction(std::move(v_req));
                    }},
                req);
            return res;
        });
    }
}
