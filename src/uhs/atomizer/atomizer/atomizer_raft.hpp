// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_ATOMIZER_ATOMIZER_RAFT_H_
#define OPENCBDC_TX_SRC_ATOMIZER_ATOMIZER_RAFT_H_

#include "messages.hpp"
#include "state_machine.hpp"
#include "util/network/connection_manager.hpp"
#include "util/raft/node.hpp"
#include "util/raft/state_manager.hpp"

namespace cbdc::atomizer {
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
        /// \param opts configuration options.
        /// \param raft_callback NuRaft callback for raft events.
        /// \param wait_for_followers true if the leader raft node should
        ///                           re-attempt to add all followers to the
        ///                           cluster until success.
        atomizer_raft(uint32_t atomizer_id,
                      const network::endpoint_t& raft_endpoint,
                      size_t stxo_cache_depth,
                      std::shared_ptr<logging::log> logger,
                      config::options opts,
                      nuraft::cb_func::func_type raft_callback,
                      bool wait_for_followers);

        /// Serialize and replicate the given request in the atomizer raft
        /// cluster. Return the response asynchronously via the given result
        /// function, if provided.
        /// \param r state machine request to replicate.
        /// \param result_fn function to call with the response, or nullptr to
        ///                  ignore the response.
        /// \return true if the replication was initiated successfully.
        auto make_request(const state_machine::request& r,
                          const raft::callback_type& result_fn) -> bool;

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
        void tx_notify(tx_notify_request&& notif);

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
        std::vector<aggregate_tx_notification> m_complete_txs;
        std::shared_ptr<logging::log> m_log;
        config::options m_opts;
    };
}

#endif // OPENCBDC_TX_SRC_ATOMIZER_ATOMIZER_RAFT_H_
