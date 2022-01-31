// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "server.hpp"

namespace cbdc::coordinator::rpc {
    server::server(
        interface* impl,
        std::unique_ptr<cbdc::rpc::async_server<request, response>> srv)
        : m_impl(impl),
          m_srv(std::move(srv)) {
        m_srv->register_handler_callback(
            [&](request req,
                std::function<void(std::optional<bool>)> callback) {
                return m_impl->execute_transaction(std::move(req),
                                                   std::move(callback));
            });
    }
}
