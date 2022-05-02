// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SENTINEL_SERVER_H_
#define OPENCBDC_TX_SRC_SENTINEL_SERVER_H_

#include "uhs/sentinel/interface.hpp"
#include "uhs/transaction/messages.hpp"
#include "util/rpc/async_server.hpp"
#include "util/rpc/format.hpp"

namespace cbdc::sentinel::rpc {
    /// RPC server for a sentinel.
    class server {
      public:
        /// Constructor. Registers the sentinel implementation with the RPC
        /// server using a request handler callback.
        /// \param impl pointer to a sentinel implementation.
        /// \param srv pointer to a blocking RPC server.
        server(
            interface* impl, // TODO: convert sentinel::controller to
                             //       contain a shared_ptr to an implementation
            std::unique_ptr<cbdc::rpc::async_server<request, response>> srv);

        ~server();

        server(const server&) = delete;
        auto operator=(const server&) -> server& = delete;
        server(server&&) = delete;
        auto operator=(server&&) -> server& = delete;

      private:
        using callback_type = std::function<void(std::optional<response>)>;
        using request_type = std::pair<request, callback_type>;

        interface* m_impl;
        std::unique_ptr<cbdc::rpc::async_server<request, response>> m_srv;
        blocking_queue<request_type> m_queue;

        std::vector<std::thread> m_threads;

        auto handle_request() -> bool;
    };
}

#endif // OPENCBDC_TX_SRC_SENTINEL_SERVER_H_
