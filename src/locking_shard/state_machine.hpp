// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_LOCKING_SHARD_STATE_MACHINE_H_
#define OPENCBDC_TX_SRC_LOCKING_SHARD_STATE_MACHINE_H_

#include "common/logging.hpp"
#include "locking_shard.hpp"
#include "rpc/blocking_server.hpp"

#include <libnuraft/nuraft.hxx>
#include <mutex>

namespace cbdc::locking_shard {
    /// Raft state machine for handling locking shard RPC requests.
    class state_machine
        : public nuraft::state_machine,
          public cbdc::rpc::blocking_server<rpc::request,
                                            rpc::response,
                                            nuraft::buffer&,
                                            nuraft::ptr<nuraft::buffer>> {
      public:
        /// Constructor.
        /// \param output_range inclusive range of hash prefixes this shard is
        ///                     responsible for.
        /// \param logger log instance.
        /// \param completed_txs_cache_size number of confirmed TX IDs to keep
        ///                                 before evicting the oldest TX ID.
        /// \param preseed_file path to file containing shard pre-seeding data
        ///                     or empty string to disable pre-seeding.
        state_machine(const std::pair<uint8_t, uint8_t>& output_range,
                      std::shared_ptr<logging::log> logger,
                      size_t completed_txs_cache_size,
                      const std::string& preseed_file);

        /// Commit the given raft log entry at the given log index, and return
        /// the result.
        /// \param log_idx raft log index of the log entry.
        /// \param data serialized RPC request.
        /// \return serialized RPC response or nullptr if there was an error
        ///         processing the request.
        auto commit(uint64_t log_idx, nuraft::buffer& data)
            -> nuraft::ptr<nuraft::buffer> override;

        /// Not implemented for locking shard.
        /// \return false.
        auto apply_snapshot(nuraft::snapshot& /* s */) -> bool override;

        /// Not implemented for locking shard.
        /// \return nullptr.
        auto last_snapshot() -> nuraft::ptr<nuraft::snapshot> override;

        /// Returns the most recently committed log entry index.
        /// \return log entry index.
        auto last_commit_index() -> uint64_t override;

        /// Not implemented for locking shard.
        void create_snapshot(
            nuraft::snapshot& /* s */,
            nuraft::async_result<bool>::handler_type& /* when_done */)
            override;

        /// Returns a pointer to the locking shard instance managed by this
        /// state machine.
        /// \return locking shard instance.
        auto get_shard_instance()
            -> std::shared_ptr<cbdc::locking_shard::locking_shard>;

      private:
        auto process_request(cbdc::locking_shard::rpc::request req)
            -> cbdc::locking_shard::rpc::response;

        std::atomic<uint64_t> m_last_committed_idx{0};
        nuraft::ptr<nuraft::snapshot> m_snapshot{};
        std::shared_mutex m_snapshots_mut{};

        nuraft::ptr<nuraft::snapshot> m_tmp_snapshot{};
        std::mutex m_tmp_mut{};

        std::shared_ptr<cbdc::locking_shard::locking_shard> m_shard{};
        std::pair<uint8_t, uint8_t> m_output_range{};
        std::string m_snapshot_dir{};
        std::string m_db_dir{};

        std::shared_ptr<logging::log> m_logger;
    };
}

#endif
