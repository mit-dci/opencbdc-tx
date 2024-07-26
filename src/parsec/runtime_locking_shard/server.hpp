// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_RUNTIME_LOCKING_SHARD_SERVER_H_
#define OPENCBDC_TX_SRC_PARSEC_RUNTIME_LOCKING_SHARD_SERVER_H_

#include "interface.hpp"
#include "messages.hpp"
#include "util/common/logging.hpp"
#include "util/rpc/async_server.hpp"

namespace cbdc::parsec::runtime_locking_shard::rpc {
    /// RPC server for a runtime locking shard.
    class server {
      public:
        /// Constructor. Registers the shard implementation with the RPC
        /// server using a request handler callback.
        /// \param logger log instance.
        /// \param impl pointer to a runtime locking shard implementation.
        /// \param repl pointer to a replicated shard implementation.
        /// \param srv pointer to an asynchronous RPC server.
        server(
            std::shared_ptr<logging::log> logger,
            std::shared_ptr<interface> impl,
            std::shared_ptr<replicated_shard_interface> repl,
            std::unique_ptr<cbdc::rpc::async_server<request, response>> srv);

      private:
        std::shared_ptr<logging::log> m_log;
        std::shared_ptr<interface> m_impl;
        std::shared_ptr<replicated_shard_interface> m_repl;
        std::unique_ptr<cbdc::rpc::async_server<request, response>> m_srv;

        using callback_type = std::function<void(std::optional<response>)>;

        auto handler_callback(const request& req,
                              callback_type callback) -> bool;

        void handle_prepare(interface::prepare_return_type ret,
                            const rpc::prepare_request& msg,
                            const callback_type& callback);

        void handle_commit(replicated_shard_interface::return_type ret,
                           rpc::commit_request msg,
                           const callback_type& callback);

        void do_rollback(replicated_shard_interface::return_type ret,
                         rpc::rollback_request msg,
                         const callback_type& callback);

        void do_finish(replicated_shard_interface::return_type ret,
                       rpc::finish_request msg,
                       const callback_type& callback);
    };
}

#endif
