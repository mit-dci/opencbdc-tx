// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COORDINATOR_SERVER_H_
#define OPENCBDC_TX_SRC_COORDINATOR_SERVER_H_

#include "interface.hpp"
#include "messages.hpp"
#include "util/rpc/async_server.hpp"

namespace cbdc::coordinator::rpc {
    /// RPC server for a coordinator.
    class server {
      public:
        /// Constructor. Registers the coordinator implementation with the RPC
        /// server using a request handler callback.
        /// \param impl pointer to a coordinator implementation.
        /// \param srv pointer to an asynchronous RPC server.
        server(
            interface* impl, // TODO: convert coordinator::controller to
                             //       contain a shared_ptr to an implementation
            std::unique_ptr<cbdc::rpc::async_server<request, response>> srv);

      private:
        interface* m_impl;
        std::unique_ptr<cbdc::rpc::async_server<request, response>> m_srv;
    };
}

#endif
