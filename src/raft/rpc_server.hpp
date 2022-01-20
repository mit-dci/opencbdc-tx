// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RAFT_RPC_SERVER_H_
#define OPENCBDC_TX_SRC_RAFT_RPC_SERVER_H_

#include "node.hpp"
#include "rpc/async_server.hpp"

namespace cbdc::raft::rpc {
    /// Generic RPC server for raft nodes for which the replicated state
    /// machine handles the request processing logic. Replicates
    /// requests to the cluster which executes them via its state machine. Once
    /// state machine execution completes, the raft node returns the result via
    /// a callback function.
    class server : public cbdc::rpc::raw_async_server {
      public:
        /// Registers the raft node whose state machine handles RPC requests
        /// for this server.
        /// \param impl pointer to the raft node.
        /// \see cbdc::rpc::server
        void register_raft_node(std::shared_ptr<node> impl) {
            m_impl = std::move(impl);
            cbdc::rpc::raw_async_server::register_handler_callback(
                [&](buffer req, response_callback_type resp_cb) {
                    return request_handler(std::move(req), std::move(resp_cb));
                });
        }

        // TODO: implement syncronous call method

      private:
        std::shared_ptr<node> m_impl;

        using response_callback_type =
            typename cbdc::rpc::raw_async_server::response_callback_type;

        auto request_handler(buffer request_buf,
                             response_callback_type response_callback)
            -> bool {
            if(!m_impl->is_leader()) {
                return false;
            }

            // TODO: make network and sockets generic over the buffer type so
            //       these copy operations for the request and response to get
            //       over the nuraft/cbdc boundary are not needed.
            auto new_log = nuraft::buffer::alloc(request_buf.size());
            nuraft::buffer_serializer bs(new_log);
            bs.put_raw(request_buf.data(), request_buf.size());

            auto success = m_impl->replicate(
                new_log,
                [&, resp_cb = std::move(response_callback), req_buf = new_log](
                    result_type& r,
                    nuraft::ptr<std::exception>& err) {
                    if(err) {
                        resp_cb(std::nullopt);
                        return;
                    }

                    const auto res = r.get();
                    if(!res) {
                        resp_cb(std::nullopt);
                        return;
                    }

                    auto resp_pkt = cbdc::buffer();
                    resp_pkt.append(res->data_begin(), res->size());
                    resp_cb(std::move(resp_pkt));
                });

            return success;
        }
    };
}

#endif
