// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RAFT_RPC_SERVER_H_
#define OPENCBDC_TX_SRC_RAFT_RPC_SERVER_H_

#include "node.hpp"
#include "util/rpc/async_server.hpp"

namespace cbdc::raft::rpc {
    using validation_callback = std::function<void(buffer, bool)>;
    using validate_function_type
        = std::function<bool(buffer, validation_callback)>;

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
            register_raft_node(std::move(impl), std::nullopt);
        }

        /// Registers the raft node whose state machine handles RPC requests
        /// for this server.
        /// \param impl pointer to the raft node.
        /// \param validate optional method to validate requests before
        /// replicating them to the raft cluster
        /// \see cbdc::rpc::server
        void
        register_raft_node(std::shared_ptr<node> impl,
                           std::optional<validate_function_type> validate) {
            m_impl = std::move(impl);
            if(validate.has_value()) {
                m_validate_func = std::move(validate.value());
            } else {
                m_validate_func
                    = [&](buffer b, const validation_callback& cb) {
                          cb(std::move(b), true);
                          return true;
                      };
            }
            cbdc::rpc::raw_async_server::register_handler_callback(
                [&](buffer req, response_callback_type resp_cb) {
                    return request_handler(std::move(req), std::move(resp_cb));
                });
        }

        // TODO: implement synchronous call method

      private:
        std::shared_ptr<node> m_impl;
        validate_function_type m_validate_func;

        using response_callback_type =
            typename cbdc::rpc::raw_async_server::response_callback_type;

        auto request_handler(buffer request_buf,
                             response_callback_type response_callback)
            -> bool {
            if(!m_impl->is_leader()) {
                return false;
            }

            return m_validate_func(
                std::move(request_buf),
                [&, res_cb = std::move(response_callback)](buffer buf2,
                                                           bool valid) {
                    if(!valid) {
                        res_cb(std::nullopt);
                        return;
                    }

                    auto new_log = nuraft::buffer::alloc(buf2.size());
                    nuraft::buffer_serializer bs(new_log);
                    bs.put_raw(buf2.data(), buf2.size());

                    auto success = m_impl->replicate(
                        new_log,
                        [&, resp_cb = res_cb, req_buf = new_log](
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
                    if(!success) {
                        res_cb(std::nullopt);
                    }
                });
        }
    };
}

#endif
