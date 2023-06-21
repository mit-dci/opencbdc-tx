// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_TICKET_MACHINE_CONTROLLER_H_
#define OPENCBDC_TX_SRC_PARSEC_TICKET_MACHINE_CONTROLLER_H_

#include "impl.hpp"
#include "state_machine.hpp"
#include "util/raft/node.hpp"
#include "util/raft/rpc_server.hpp"
#include "util/rpc/tcp_server.hpp"

namespace cbdc::parsec::ticket_machine {
    /// Manages a replicated ticket machine using Raft.
    class controller {
      public:
        /// Constructor.
        /// \param node_id node ID within the cluster.
        /// \param server_endpoint endpoint to listen for RPC requests on.
        /// \param raft_endpoints vector of endpoints for the raft nodes in the
        ///                       cluster.
        /// \param logger log to use for output.
        controller(size_t node_id,
                   network::endpoint_t server_endpoint,
                   std::vector<network::endpoint_t> raft_endpoints,
                   std::shared_ptr<logging::log> logger);
        ~controller() = default;

        controller() = delete;
        controller(const controller&) = delete;
        auto operator=(const controller&) -> controller& = delete;
        controller(controller&&) = delete;
        auto operator=(controller&&) -> controller& = delete;

        /// Initializes the ticket machine. Starts the raft instance and joins
        /// the raft cluster.
        /// \return true if initialization was successful.
        auto init() -> bool;

      private:
        auto raft_callback(nuraft::cb_func::Type type,
                           nuraft::cb_func::Param* param)
            -> nuraft::cb_func::ReturnCode;

        std::shared_ptr<logging::log> m_logger;

        std::shared_ptr<state_machine> m_state_machine;
        std::shared_ptr<raft::node> m_raft_serv;
        std::unique_ptr<cbdc::rpc::tcp_server<raft::rpc::server>> m_server;

        std::vector<network::endpoint_t> m_raft_endpoints;
        network::endpoint_t m_server_endpoint;

        static constexpr auto m_batch_size = 1000;
    };
}

#endif
