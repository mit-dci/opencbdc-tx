// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "status_server.hpp"

#include "util/common/variant_overloaded.hpp"

namespace cbdc::locking_shard::rpc {
    status_server::status_server(
        std::shared_ptr<status_interface> impl,
        std::unique_ptr<
            cbdc::rpc::blocking_server<status_request, status_response>> srv)
        : m_impl(std::move(impl)),
          m_srv(std::move(srv)) {
        m_srv->register_handler_callback([&](status_request req) {
            return request_handler(req);
        });
    }

    auto status_server::request_handler(status_request req)
        -> std::optional<status_response> {
        return std::visit(overloaded{[&](const uhs_status_request& r) {
                                         return m_impl->check_unspent(
                                             r.m_uhs_id);
                                     },
                                     [&](const tx_status_request& r) {
                                         return m_impl->check_tx_id(r.m_tx_id);
                                     }},
                          req);
    }
}
