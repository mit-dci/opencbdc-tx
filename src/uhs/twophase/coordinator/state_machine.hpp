// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COORDINATOR_STATE_MACHINE_H_
#define OPENCBDC_TX_SRC_COORDINATOR_STATE_MACHINE_H_

#include "util/common/hashmap.hpp"
#include "util/common/logging.hpp"

#include <libnuraft/nuraft.hxx>
#include <unordered_map>
#include <unordered_set>

namespace cbdc::coordinator {
    /// \brief Raft state machine for managing a replicated coordinator.
    ///
    /// Contains a \ref coordinator_state and the last-committed index.
    /// Accepts requests to manage and query distributed transactions.
    class state_machine final : public nuraft::state_machine {
      public:
        /// Constructor.
        /// Constructs a new coordinator state machine.
        ///
        /// \param logger pointer to logger instance.
        explicit state_machine(std::shared_ptr<logging::log> logger);

        /// Types of command the state machine can process.
        enum class command : uint8_t {
            prepare = 0, ///< Stores a dtx in the prepare phase.
            commit = 1,  ///< Moves a dtx from prepare to commit.
            discard = 2, ///< Moves a dtx from commit to discard.
            done = 3,    ///< Clears the dtx from the coordinator state.
            get = 4      ///< Retrieves all active dtxs.
        };

        /// Used to store dtxs, which phase they are in and relevant data
        /// require for recovery. Each dtx should only be in one of the
        /// constituent variables at a time.
        struct coordinator_state {
            /// Maps dtx IDs in the prepare phase to a byte array containing
            /// relevant data for recovery.
            std::unordered_map<hash_t,
                               nuraft::ptr<nuraft::buffer>,
                               cbdc::hashing::const_sip_hash<hash_t>>
                m_prepare_txs{};
            /// Maps dtx IDs in the commit phase to a byte array containing
            /// relevant data for recovery.
            std::unordered_map<hash_t,
                               nuraft::ptr<nuraft::buffer>,
                               cbdc::hashing::const_sip_hash<hash_t>>
                m_commit_txs{};
            /// Set of dtx IDs in the discard phase.
            std::unordered_set<hash_t, cbdc::hashing::const_sip_hash<hash_t>>
                m_discard_txs{};
        };

        /// Commits a state machine command.
        ///
        /// \param log_idx index of raft entry.
        /// \param data the buffer containing the command to commit.
        /// \return pointer to a buffer with the serialized execution result
        ///         or nullptr.
        auto commit(uint64_t log_idx, nuraft::buffer& data)
            -> nuraft::ptr<nuraft::buffer> override;

        /// Not implemented for coordinators.
        /// \return false (unconditionally).
        auto apply_snapshot(nuraft::snapshot& /* s */) -> bool override;

        /// Not implemented for coordinators.
        /// \return nullptr (unconditionally).
        auto last_snapshot() -> nuraft::ptr<nuraft::snapshot> override;

        /// Returns the index of the last-committed command.
        auto last_commit_index() -> uint64_t override;

        /// Not implemented for coordinators.
        void create_snapshot(
            nuraft::snapshot& /* s */,
            nuraft::async_result<bool>::handler_type& when_done) override;

      private:
        std::atomic<uint64_t> m_last_committed_idx{0};
        coordinator_state m_state{};
        std::shared_ptr<logging::log> m_logger;
    };
}

#endif // OPENCBDC_TX_SRC_COORDINATOR_STATE_MACHINE_H_
