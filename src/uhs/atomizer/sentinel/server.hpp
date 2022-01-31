// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SENTINEL_SERVER_H_
#define OPENCBDC_TX_SRC_SENTINEL_SERVER_H_

#include "uhs/sentinel/interface.hpp"
#include "uhs/transaction/messages.hpp"
#include "util/rpc/blocking_server.hpp"
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
            std::unique_ptr<cbdc::rpc::blocking_server<request, response>>
                srv);

      private:
        interface* m_impl;
        std::unique_ptr<cbdc::rpc::blocking_server<request, response>> m_srv;
    };
}

#endif // OPENCBDC_TX_SRC_SENTINEL_SERVER_H_
