// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_ATOMIZER_STATE_MACHINE_H_
#define OPENCBDC_TX_SRC_ATOMIZER_STATE_MACHINE_H_

#include "atomizer.hpp"
#include "messages.hpp"

#include <libnuraft/nuraft.hxx>
#include <shared_mutex>

namespace cbdc::atomizer {
    /// \brief Raft state machine for managing a replicated atomizer.
    ///
    /// Contains a \ref atomizer and a cache of recently created blocks.
    /// Accepts requests to retrieve and prune recent blocks from the cache.
    class state_machine : public nuraft::state_machine {
      public:
        /// Constructor.
        /// \param stxo_cache_depth depth of the spent transaction output
        ///                         cache, passed to the atomizer.
        /// \param snapshot_dir path to directory in which to store snapshots.
        ///                     Will create the directory if it doesn't exist.
        state_machine(size_t stxo_cache_depth, std::string snapshot_dir);

        /// Atomizer state machine request.
        using request = std::variant<aggregate_tx_notify_request,
                                     make_block_request,
                                     get_block_request,
                                     prune_request>;

        /// Atomizer state machine response.
        using response
            = std::variant<make_block_response, get_block_response, errors>;

        /// Executes the committed the raft log entry at the given index and
        /// return the state machine execution result.
        /// \param log_idx index of the given log entry.
        /// \param data serialized log entry containing state machine command
        ///             and parameters.
        /// \return serialized execution result.
        [[nodiscard]] auto commit(nuraft::ulong log_idx, nuraft::buffer& data)
            -> nuraft::ptr<nuraft::buffer> override;

        /// Handler for the raft cluster configuration changes
        /// \param log_idx Raft log number of the configuration change.
        void commit_config(
            nuraft::ulong log_idx,
            nuraft::ptr<nuraft::cluster_config>& /*new_conf*/) override;

        /// Read the portion of the state machine snapshot associated with
        /// the given metadata and object ID into a buffer.
        /// \param s metadata of snapshot to read.
        /// \param user_snp_ctx pointer to a snapshot context; must be provided
        ///                     to all successive calls to this method for the
        ///                     same snapshot.
        /// \param obj_id ID of the snapshot object to read.
        /// \param data_out buffer in which to write the snapshot object.
        /// \param is_last_obj set to true if this object ID is the last
        ///                    snapshot object.
        /// \return 0 if the object was read successfully.
        [[nodiscard]] auto
        read_logical_snp_obj(nuraft::snapshot& s,
                             void*& user_snp_ctx,
                             nuraft::ulong obj_id,
                             nuraft::ptr<nuraft::buffer>& data_out,
                             bool& is_last_obj) -> int override;

        /// Saves the portion of the state machine snapshot associated with
        /// the given metadata and object ID into persistent storage.
        /// \param s metadata of snapshot to save.
        /// \param obj_id ID of the snapshot object to save.
        /// \param data buffer from which to read the snapshot object data to
        ///             save.
        /// \param is_first_obj true if this object ID is the first snapshot
        ///                     object.
        /// \param is_last_obj true if this object ID is the last snapshot
        ///                    object.
        void save_logical_snp_obj(nuraft::snapshot& s,
                                  nuraft::ulong& obj_id,
                                  nuraft::buffer& data,
                                  bool is_first_obj,
                                  bool is_last_obj) override;

        /// Replaces the state of the state machine with the state stored in
        /// the snapshot referenced by the given snapshot metadata.
        /// \param s snapshot metadata.
        /// \return true if the operation successfully applied the snapshot.
        [[nodiscard]] auto apply_snapshot(nuraft::snapshot& s)
            -> bool override;

        /// Returns the most recent snapshot metadata.
        /// \return snapshot metadata, or nullptr if there is no snapshot.
        [[nodiscard]] auto last_snapshot()
            -> nuraft::ptr<nuraft::snapshot> override;

        /// Returns the index of the most recently committed log entry.
        /// \return log index.
        [[nodiscard]] auto last_commit_index() -> nuraft::ulong override;

        /// Creates a snapshot with the given metadata.
        /// \param s snapshot metadata.
        /// \param when_done function to call when snapshot creation is
        ///                  complete.
        void create_snapshot(
            nuraft::snapshot& s,
            nuraft::async_result<bool>::handler_type& when_done) override;

        /// Returns the total number of transaction notifications which the
        /// state machine has processed.
        /// \return transaction notification count.
        [[nodiscard]] auto tx_notify_count() -> uint64_t;

        /// Maps block heights to blocks.
        using blockstore_t
            = std::unordered_map<uint64_t, cbdc::atomizer::block>;

        /// Represents a snapshot of the state machine with associated
        /// metadata.
        struct snapshot {
            /// Pointer to the atomizer instance.
            std::shared_ptr<cbdc::atomizer::atomizer> m_atomizer;
            /// Pointer to the nuraft snapshot metadata.
            nuraft::ptr<nuraft::snapshot> m_snp{};
            /// Pointer to the state of the block cache.
            std::shared_ptr<blockstore_t> m_blocks{};
        };

      private:
        [[nodiscard]] auto get_snapshot_path(uint64_t idx) const
            -> std::string;

        [[nodiscard]] auto get_tmp_path() const -> std::string;

        [[nodiscard]] auto read_snapshot(uint64_t idx)
            -> std::optional<snapshot>;

        static constexpr auto m_tmp_file = "tmp";

        std::atomic<uint64_t> m_last_committed_idx{0};

        std::shared_ptr<cbdc::atomizer::atomizer> m_atomizer;
        std::shared_ptr<blockstore_t> m_blocks;

        std::atomic<uint64_t> m_tx_notify_count{0};

        std::string m_snapshot_dir;

        size_t m_stxo_cache_depth{};

        std::shared_mutex m_snp_mut;
    };
}
#endif // OPENCBDC_TX_SRC_ATOMIZER_STATE_MACHINE_H_
