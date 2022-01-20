// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_LOCKING_SHARD_STATUS_SERVER_H_
#define OPENCBDC_TX_SRC_LOCKING_SHARD_STATUS_SERVER_H_

#include "rpc/blocking_server.hpp"
#include "status_interface.hpp"
#include "status_messages.hpp"

#include <memory>

namespace cbdc::locking_shard::rpc {
    /// Server for handling TX and UHS ID status requests.
    class status_server {
      public:
        /// Constructor.
        /// \param impl pointer to an implementation of the locking shard status
        ///             interface.
        /// \param srv pointer to an initialized RPC server which is ready to accept requests.
        status_server(
            std::shared_ptr<status_interface> impl,
            std::unique_ptr<cbdc::rpc::blocking_server<status_request,
                                                       status_response>> srv);

      private:
        std::shared_ptr<status_interface> m_impl;
        std::unique_ptr<
            cbdc::rpc::blocking_server<status_request, status_response>>
            m_srv;

        auto request_handler(status_request req)
            -> std::optional<status_response>;
    };
}

#endif
