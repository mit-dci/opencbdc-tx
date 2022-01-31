// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COORDINATOR_CONTROLLER_H_
#define OPENCBDC_TX_SRC_COORDINATOR_CONTROLLER_H_

#include "distributed_tx.hpp"
#include "interface.hpp"
#include "server.hpp"
#include "state_machine.hpp"
#include "uhs/twophase/locking_shard/locking_shard.hpp"
#include "util/common/buffer.hpp"
#include "util/common/random_source.hpp"
#include "util/network/connection_manager.hpp"
#include "util/raft/node.hpp"

namespace cbdc::coordinator {
    /// Replicated coordinator node. Participates in a raft cluster with
    /// other replicated coordinators. When acting as the leader, listens
    /// on a specified endpoint and handles new transaction requests from
    /// sentinels. Recovers failed dtxs as part of its leadership
    /// transition.
    class controller : public interface {
      public:
        using send_fn_t = std::function<void(const std::shared_ptr<buffer>&,
                                             cbdc::network::peer_id_t)>;

        /// Constructs a new replicated coordinator node.
        /// \param node_id raft node ID in the coordinator cluster.
        /// \param coordinator_id ID to determine which coordinator cluster
        ///                       this node is part of.
        /// \param opts configuration options.
        /// \param logger pointer to shared logger.
        controller(size_t node_id,
                   size_t coordinator_id,
                   cbdc::config::options opts,
                   std::shared_ptr<logging::log> logger);

        controller() = delete;
        controller(const controller&) = delete;
        auto operator=(const controller&) -> controller& = delete;
        controller(controller&&) = delete;
        auto operator=(controller&&) -> controller& = delete;

        ~controller() override;

        /// Starts the replicated coordinator and associated raft server.
        /// \return true if the initialization succeeded.
        auto init() -> bool;

        /// Terminates the replicated coordinator instance.
        void quit();

        /// List of compact transactions associated with a distributed
        /// transaction in the prepare phase.
        using prepare_tx = std::vector<transaction::compact_tx>;

        /// Map from distributed transaction IDs in the prepare phase to the
        /// associated compact transactions.
        using prepare_txs = std::
            unordered_map<hash_t, prepare_tx, hashing::const_sip_hash<hash_t>>;

        /// Aggregated responses and metadata from the prepare phase. First is
        /// a vector of bools, true if the transaction at the same index in the
        /// batch should be completed, false if it should be aborted. Second is
        /// a vector, one element for each shard ID, specifying which
        /// transaction indexes in the batch are revelant to the shard.
        using commit_tx
            = std::pair<std::vector<bool>, std::vector<std::vector<uint64_t>>>;

        /// Map from distributed transaction IDs in the commit phase to the
        /// associated responses and metadata from the prepare phase.
        using commit_txs = std::
            unordered_map<hash_t, commit_tx, hashing::const_sip_hash<hash_t>>;

        /// Set of distributed transaction IDs in the discard phase.
        using discard_txs
            = std::unordered_set<hash_t, hashing::const_sip_hash<hash_t>>;

        /// Metadata of a command for the state machine.
        struct sm_command_header {
            /// The type of command.
            state_machine::command m_comm{};

            /// The ID of the distributed transaction the command applies to,
            /// if applicable.
            std::optional<hash_t> m_dtx_id{};

            auto operator==(const sm_command_header& rhs) const -> bool;
        };

        /// A full command for the state machine to process.
        struct sm_command {
            /// The command's metadata.
            sm_command_header m_header{};

            /// Associated transactions to prepare or commit, if applicable.
            std::optional<std::variant<prepare_tx, commit_tx>> m_data{};
        };

        /// \brief Current state of distributed transactions managed by a
        ///        coordinator.
        ///
        /// Includes lists of transactions upon which the coordinator is
        /// operating.
        struct coordinator_state {
            /// Transactions in the prepare phase.
            prepare_txs m_prepare_txs{};

            /// Transactions in the commit phase.
            commit_txs m_commit_txs{};

            /// Transactions in the discard phase.
            discard_txs m_discard_txs{};

            auto operator==(const coordinator_state& rhs) const -> bool;
        };

        /// \brief Coordinates a transaction among locking shards.
        /// Adds a transaction to the current batch. Registers a callback
        /// function to return the transaction execution result once the shards
        /// completely process the batch.
        /// \param tx transaction to execute.
        /// \param result_callback function to call with the result once
        ///                        execution is complete.
        /// \return true if the current batch now contains the transaction.
        ///         false if the current batch already contained the
        ///         transaction or if the controller shut down before the
        ///         operation could finish.
        auto execute_transaction(transaction::compact_tx tx,
                                 callback_type result_callback)
            -> bool override;

      private:
        size_t m_node_id;
        size_t m_coordinator_id;
        cbdc::config::options m_opts;
        std::shared_ptr<logging::log> m_logger;

        nuraft::ptr<state_machine> m_state_machine;
        raft::node m_raft_serv;
        nuraft::raft_params m_raft_params{};
        std::atomic_bool m_running{false};
        std::vector<std::shared_ptr<cbdc::locking_shard::interface>> m_shards;
        std::vector<std::vector<network::endpoint_t>> m_shard_endpoints;
        std::vector<cbdc::config::shard_range_t> m_shard_ranges;
        random_source m_rnd{config::random_source};
        std::mutex m_batch_mut;
        std::condition_variable m_batch_cv;
        std::shared_ptr<distributed_tx> m_current_batch;
        std::shared_ptr<std::unordered_map<hash_t,
                                           std::pair<callback_type, size_t>,
                                           hashing::const_sip_hash<hash_t>>>
            m_current_txs;
        size_t m_batch_size;
        std::shared_mutex m_shards_mut;
        std::thread m_batch_exec_thread;
        std::unique_ptr<rpc::server> m_rpc_server;
        network::endpoint_t m_handler_endpoint;
        std::vector<std::pair<std::shared_ptr<std::thread>, std::atomic_bool>>
            m_exec_threads;
        std::shared_mutex m_exec_mut;

        std::thread m_start_thread;
        bool m_start_flag{false};
        bool m_stop_flag{false};
        std::condition_variable m_start_cv;
        std::mutex m_start_mut;
        bool m_quit{false};

        void start_stop_func();

        void start();

        void stop();

        auto recovery_func() -> bool;

        void batch_executor_func();

        auto raft_callback(nuraft::cb_func::Type type,
                           nuraft::cb_func::Param* param)
            -> nuraft::cb_func::ReturnCode;

        auto prepare_cb(const hash_t& dtx_id,
                        const std::vector<transaction::compact_tx>& txs)
            -> bool;
        auto commit_cb(const hash_t& dtx_id,
                       const std::vector<bool>& complete_txs,
                       const std::vector<std::vector<uint64_t>>& tx_idxs)
            -> bool;
        auto discard_cb(const hash_t& dtx_id) -> bool;
        auto done_cb(const hash_t& dtx_id) -> bool;

        void batch_set_cbs(distributed_tx& c);

        [[nodiscard]] auto replicate_sm_command(const sm_command& c)
            -> std::optional<nuraft::ptr<nuraft::buffer>>;

        void connect_shards();

        void schedule_exec(std::function<void(size_t)>&& f);

        void join_execs();
    };
}

#endif // OPENCBDC_TX_SRC_COORDINATOR_CONTROLLER_H_
