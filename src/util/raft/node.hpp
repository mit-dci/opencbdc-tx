// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RAFT_NODE_H_
#define OPENCBDC_TX_SRC_RAFT_NODE_H_

#include "console_logger.hpp"
#include "state_manager.hpp"
#include "util/common/config.hpp"
#include "util/network/connection_manager.hpp"

#include <libnuraft/nuraft.hxx>

namespace cbdc::raft {
    /// A NuRaft state machine execution result.
    using result_type = nuraft::cmd_result<nuraft::ptr<nuraft::buffer>>;

    /// Function type for raft state machine execution result callbacks.
    using callback_type
        = std::function<void(result_type& r,
                             nuraft::ptr<std::exception>& err)>;

    /// \brief A node in a raft cluster.
    ///
    /// Wrapper for replicated state machine functionality using raft from the
    /// external library NuRaft. Builds a cluster with other raft
    /// nodes. Uses NuRaft to durably replicate log entries between a quorum of
    /// raft nodes. Callers provide a state machine to execute the log entries
    /// and return the execution result.
    class node {
      public:
        node() = delete;
        node(const node&) = delete;
        auto operator=(const node&) -> node& = delete;
        node(node&&) = delete;
        auto operator=(node&&) -> node& = delete;

        /// Constructor.
        /// \param node_id identifier of the node in the raft cluster. Must be
        ///                0 or greater.
        /// \param raft_endpoint TCP endpoint upon which to listen for incoming raft
        ///                      connections.
        /// \param node_type name of the raft cluster this node will be part
        ///                  of.
        /// \param blocking true if replication calls should block until the
        ///                 state machine makes an execution result available.
        /// \param sm pointer to the state machine replicated by the cluster.
        /// \param asio_thread_pool_size number of threads for processing raft
        ///                              messages. Set to 0 to use the number
        ///                              of cores on the system.
        /// \param logger log instance NuRaft should use.
        /// \param raft_cb NuRaft callback to report raft events.
        node(int node_id,
             const network::endpoint_t& raft_endpoint,
             const std::string& node_type,
             bool blocking,
             nuraft::ptr<nuraft::state_machine> sm,
             size_t asio_thread_pool_size,
             std::shared_ptr<logging::log> logger,
             nuraft::cb_func::func_type raft_cb);

        ~node();

        /// Initializes the NuRaft instance with the given state machine and
        /// raft parameters.
        /// \param raft_params NuRaft-specific parameters for the raft node.
        /// \return true if the raft node initialized successfully.
        auto init(const nuraft::raft_params& raft_params) -> bool;

        /// Connect to each of the given raft nodes and join them to the
        /// cluster. If this node is not node 0, this method blocks until
        /// node 0 joins this node to the cluster.
        /// \param raft_servers node endpoints of the other raft nodes in the
        ///                     cluster.
        /// \return true if adding the nodes to the raft cluster succeeded.
        auto
        build_cluster(const std::vector<network::endpoint_t>& raft_servers)
            -> bool;

        /// Indicates whether this node is the current raft leader.
        /// \return true if this node is the leader.
        [[nodiscard]] auto is_leader() const -> bool;

        /// Replicates the given log entry in the cluster. Calls the given
        /// callback with the state machine execution result.
        /// \param new_log log entry to replicate.
        /// \param result_fn callback function to call asynchronously with the
        ///                  state machine execution result.
        /// \return true if the log entry was accepted for replication.
        [[nodiscard]] auto replicate(nuraft::ptr<nuraft::buffer> new_log,
                                     const callback_type& result_fn) const
            -> bool;

        /// Replicates the provided log entry and returns the results from the
        /// state machine if the replication was successful. The method will
        /// block until the result is available or replication has failed.
        /// \param new_log raft log entry to replicate
        /// \return result from state machine or empty optional if replication
        ///         failed
        [[nodiscard]] auto
        replicate_sync(const nuraft::ptr<nuraft::buffer>& new_log) const
            -> std::optional<nuraft::ptr<nuraft::buffer>>;

        /// Returns the last replicated log index.
        /// \return log index.
        [[nodiscard]] auto last_log_idx() const -> uint64_t;

        /// Returns a pointer to the state machine replicated by this raft
        /// node.
        /// \return pointer to the state machine.
        [[nodiscard]] auto get_sm() const -> nuraft::state_machine*;

        /// Shut down the NuRaft instance.
        void stop();

      private:
        uint32_t m_node_id;
        bool m_blocking;
        int m_port;

        nuraft::ptr<nuraft::logger> m_raft_logger;
        nuraft::ptr<state_manager> m_smgr;
        nuraft::ptr<nuraft::state_machine> m_sm;
        nuraft::raft_launcher m_launcher;
        nuraft::ptr<nuraft::raft_server> m_raft_instance;

        nuraft::asio_service::options m_asio_opt;
        nuraft::raft_server::init_options m_init_opts;

        [[nodiscard]] auto add_cluster_nodes(
            const std::vector<network::endpoint_t>& raft_servers) const
            -> bool;
    };
}

#endif // OPENCBDC_TX_SRC_RAFT_NODE_H_
