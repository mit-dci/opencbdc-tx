// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_LOCKING_SHARD_CONTROLLER_H_
#define OPENCBDC_TX_SRC_LOCKING_SHARD_CONTROLLER_H_

#include "client.hpp"
#include "locking_shard.hpp"
#include "state_machine.hpp"
#include "status_server.hpp"
#include "util/common/blocking_queue.hpp"
#include "util/raft/node.hpp"
#include "util/raft/rpc_server.hpp"
#include "util/rpc/tcp_server.hpp"

namespace cbdc::locking_shard {
    /// Manages a replicated locking shard using Raft.
    class controller {
      public:
        /// Constructor.
        /// \param shard_id shard cluster ID.
        /// \param node_id node ID within shard cluster.
        /// \param opts configuration parameters.
        /// \param logger log to use for output.
        controller(size_t shard_id,
                   size_t node_id,
                   cbdc::config::options opts,
                   std::shared_ptr<logging::log> logger);

        ~controller();

        controller() = delete;
        controller(const controller&) = delete;
        auto operator=(const controller&) -> controller& = delete;
        controller(controller&&) = delete;
        auto operator=(controller&&) -> controller& = delete;

        /// Initializes the locking shard by reading the pre-seed file if
        /// applicable, initializing the raft cluster, and starting listeners
        /// on the client and status client endpoints.
        /// Opens the audit log and starts a periodic supply auditing thread.
        /// \return false if initialization fails.
        auto init() -> bool;

      private:
        auto raft_callback(nuraft::cb_func::Type type,
                           nuraft::cb_func::Param* param)
            -> nuraft::cb_func::ReturnCode;
        auto validate_request(cbdc::buffer request,
                              const cbdc::raft::rpc::validation_callback& cb)
            -> bool;

        auto enqueue_validation(cbdc::buffer request,
                                cbdc::raft::rpc::validation_callback cb)
            -> bool;
        void validation_worker();

        config::options m_opts;
        std::shared_ptr<logging::log> m_logger;
        size_t m_shard_id;
        size_t m_node_id;
        std::string m_preseed_dir;
        std::vector<std::thread> m_validation_threads;
        using validation_request
            = std::pair<cbdc::buffer, cbdc::raft::rpc::validation_callback>;
        blocking_queue<validation_request> m_validation_queue;
        std::atomic<bool> m_running{true};

        std::shared_ptr<state_machine> m_state_machine;
        std::shared_ptr<locking_shard> m_shard;
        std::shared_ptr<raft::node> m_raft_serv;
        std::unique_ptr<rpc::status_server> m_status_server;
        std::unique_ptr<cbdc::rpc::tcp_server<raft::rpc::server>> m_server;
    };
}

#endif // OPENCBDC_TX_SRC_LOCKING_SHARD_CONTROLLER_H_
