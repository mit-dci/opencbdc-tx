// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SENTINEL_2PC_SERVER_H_
#define OPENCBDC_TX_SRC_SENTINEL_2PC_SERVER_H_

#include "uhs/sentinel/async_interface.hpp"
#include "uhs/transaction/messages.hpp"
#include "util/common/blocking_queue.hpp"
#include "util/rpc/async_server.hpp"
#include "util/rpc/format.hpp"

namespace cbdc::sentinel::rpc {
    struct request_queue_t {
        request m_req;
        async_interface::result_callback_type m_cb;
    };

    bool operator<(const request_queue_t& a, const request_queue_t& b);
    /// Asynchronous RPC server for a sentinel.
    class async_server {
      public:
        /// Constructor. Registers the sentinel implementation with the RPC
        /// server using a request handler callback.
        /// \param impl pointer to a sentinel implementation.
        /// \param srv pointer to a asynchronous RPC server.
        async_server(
            async_interface* impl, // TODO: convert sentinel_2pc::controller to
                                   //      contain a shared_ptr to an
                                   //      implementation
            std::unique_ptr<cbdc::rpc::async_server<request, response>> srv);

        ~async_server();

      private:
        void process();

        async_interface* m_impl;
        std::unique_ptr<cbdc::rpc::async_server<request, response>> m_srv;

        blocking_priority_queue<request_queue_t, std::less<request_queue_t>>
            m_request_queue{};
        std::thread m_processing_thread;
        std::atomic<bool> m_running = true;
    };
}

#endif // OPENCBDC_TX_SRC_SENTINEL_2PC_SERVER_H_
