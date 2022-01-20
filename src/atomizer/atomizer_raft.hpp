// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_ATOMIZER_ATOMIZER_RAFT_H_
#define OPENCBDC_TX_SRC_ATOMIZER_ATOMIZER_RAFT_H_

#include "network/connection_manager.hpp"
#include "raft/node.hpp"
#include "raft/state_manager.hpp"
#include "state_machine.hpp"

namespace cbdc::atomizer {
    /// \brief Transaction notification message.
    ///
    /// Sent from shards to the atomizer. Notifies the atomizer that a shard
    /// has received a transaction from a sentinel. The shard attaches an
    /// attestation for each transaction input that is covered by the shard's
    /// UHS subset, and currently unspent in the UHS. The shard also attaches
    /// the block height at which the attestations are valid.
    struct tx_notify_message {
        auto operator==(const tx_notify_message& rhs) const -> bool;

        /// Compact transaction associated with the notification.
        transaction::compact_tx m_tx;
        /// Set of input indexes the shard is attesting are unspent at the
        /// given block height.
        std::unordered_set<uint64_t> m_attestations;
        /// Block height at which the given input attestations are valid.
        uint64_t m_block_height{};
    };

    /// \brief Transaction notification message with a full set of input
    ///        attestations.
    ///
    /// The atomizer manager ( \ref atomizer_raft ) sends this message to the
    /// atomizer state machine ( \ref state_machine ) once it has received a
    /// full set of input attestations for a given compact transaction. The
    /// atomizer manager attaches the block height of the oldest attesation
    /// used to build the full set. The structure is used as an optimization
    /// to remove the need to replicate individual transaction notifications
    /// in the atomizer cluster.
    struct aggregate_tx_notify {
        auto operator==(const aggregate_tx_notify& rhs) const -> bool;

        /// Compact transaction associated with the notification.
        transaction::compact_tx m_tx;
        /// Block height of the oldest input attestation used to build this
        /// aggregate notification.
        uint64_t m_oldest_attestation{};
    };

    /// \brief Batch of aggregate transaction notifications.
    ///
    /// Atomizer state machine ( \ref state_machine ) message containing a
    /// batch of \ref aggregate_tx_notify.
    struct aggregate_tx_notify_set {
        auto operator==(const aggregate_tx_notify_set& rhs) const -> bool;

        // TODO: refactor this struct and other state machine operations to use
        ///      raft::rpc::server rather than explicit command bytes.
        /// State machine command, always
        /// \ref state_machine::command::tx_notify
        state_machine::command m_cmd{};
        /// Batch of aggregate transaction notifications.
        std::vector<aggregate_tx_notify> m_agg_txs;
    };

    /// \brief Manager for an atomizer raft node.
    ///
    /// Handles initialization of an atomizer state machine and associated raft
    /// node. Replicates commands to the atomizer cluster and returns the state
    /// machine execution result via a callback function once available.
    class atomizer_raft : public cbdc::raft::node {
      public:
        /// Constructor.
        /// \param atomizer_id ID of the raft node.
        /// \param raft_endpoint endpoint for raft communications.
        /// \param stxo_cache_depth number of blocks in the spent output cache.
        /// \param logger log instance.
        /// \param raft_callback NuRaft callback for raft events.
        atomizer_raft(uint32_t atomizer_id,
                      const network::endpoint_t& raft_endpoint,
                      size_t stxo_cache_depth,
                      std::shared_ptr<logging::log> logger,
                      nuraft::cb_func::func_type raft_callback);

        /// Replicate a make block command and call the result function with
        /// the generated block once available.
        /// \param result_fn function to call with the serialized block.
        /// \return true if the command was accepted for replication.
        [[nodiscard]] auto make_block(const raft::callback_type& result_fn)
            -> bool;

        /// Replicate the given serialized get block command and return the
        /// result via a callback function.
        /// \param pkt serialized get block state machine command.
        /// \param result_fn function to call with the state machine execution
        ///                  result once available.
        /// \return true if the command was accepted for replication.
        [[nodiscard]] auto get_block(const cbdc::network::message_t& pkt,
                                     const raft::callback_type& result_fn)
            -> bool;

        /// Replicate the given serialize prune command.
        /// \param pkt serialized prune command.
        void prune(const cbdc::network::message_t& pkt);

        /// Return a pointer to the state machine replicated by this raft node.
        /// \return state machine pointer.
        [[nodiscard]] auto get_sm() -> state_machine*;

        /// Return the number of transaction notifications handled by the state
        /// machine.
        /// \return number of transaction notifications.
        [[nodiscard]] auto tx_notify_count() -> uint64_t;

        /// Add the given transaction notification to the set of pending
        /// notifications. If the notification can be combined with previously
        /// received notifications to create an aggregate notification with a
        /// full set of input attestations, create an aggregate notification
        /// and add it to a list of complete transactions.
        /// \param notif transaction notification.
        void tx_notify(tx_notify_message&& notif);

        /// Replicate a transaction notification command in the state machine
        /// containing the current set of complete transactions.
        /// \param result_fn function to call with the state machine execution
        ///                  result.
        /// \return true if the command was accepted for replication.
        [[nodiscard]] auto
        send_complete_txs(const raft::callback_type& result_fn) -> bool;

      private:
        static constexpr const auto m_node_type = "atomizer";

        using attestation = std::pair<uint64_t, uint64_t>;

        struct attestation_hash {
            auto operator()(const attestation& pair) const -> size_t;
        };

        struct attestation_cmp {
            auto operator()(const attestation& a, const attestation& b) const
                -> bool;
        };

        using attestation_set = std::
            unordered_set<attestation, attestation_hash, attestation_cmp>;

        std::unordered_map<transaction::compact_tx,
                           attestation_set,
                           transaction::compact_tx_hasher>
            m_txs;
        std::mutex m_complete_mut;
        std::vector<aggregate_tx_notify> m_complete_txs;
    };
}

#endif // OPENCBDC_TX_SRC_ATOMIZER_ATOMIZER_RAFT_H_
