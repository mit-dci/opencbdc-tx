// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COORDINATOR_DISTRIBUTED_TX_H_
#define OPENCBDC_TX_SRC_COORDINATOR_DISTRIBUTED_TX_H_

#include "common/random_source.hpp"
#include "locking_shard/locking_shard.hpp"
#include "raft/node.hpp"
#include "state_machine.hpp"
#include "transaction/transaction.hpp"

#include <memory>
#include <vector>

namespace cbdc::coordinator {
    /// Class to manage a single distributed transaction (dtx) batch between
    /// shards. Capable of recovering previously failed dtxs and sharing the
    /// results of each dtx phase with callback functions (usually for
    /// replication).
    class distributed_tx {
      public:
        /// Constructs a new transaction coordinator instance
        /// \param dtx_id dtx ID for this transaction batch
        /// \param shards vector of locking shards that will participate in the
        ///               dtx. If recovering a previous dtx, the list must
        ///               refer to the same shards in the same order.
        /// \param logger logger for messages.
        distributed_tx(
            const hash_t& dtx_id,
            std::vector<std::shared_ptr<locking_shard::interface>> shards,
            std::shared_ptr<logging::log> logger);

        /// Executes the dtx batch to completion or failure, either from start,
        /// or an intermediate state if one of the recover functions were used.
        /// \return empty optional if the dtx failed, or a vector of flags
        ///         indicating which constituent transactions settled and which
        ///         were rolled back by the transaction's index in the batch.
        [[nodiscard]] auto execute() -> std::optional<std::vector<bool>>;

        /// Adds a TX to the batch managed by this coordinator and dtx ID.
        /// Should not be used after calling execute().
        /// \param tx compact transaction to add
        /// \return the index of the transaction withing the dtx batch
        auto add_tx(const transaction::compact_tx& tx) -> size_t;

        /// Returns the dtx ID associated with this coordinator instance
        /// \return dtx ID for this coordinator
        [[nodiscard]] auto get_id() const -> hash_t;

        using discard_cb_t = std::function<bool(const hash_t&)>;
        using done_cb_t = std::function<bool(const hash_t&)>;
        using commit_cb_t
            = std::function<bool(const hash_t&,
                                 const std::vector<bool>&,
                                 const std::vector<std::vector<uint64_t>>&)>;
        using prepare_cb_t
            = std::function<bool(const hash_t&,
                                 const std::vector<transaction::compact_tx>&)>;

        /// Registers a callback to be called before starting the prepare phase
        /// of the dtx
        /// \param cb callback function taking the dtx ID and a vector of
        ///           transactions in the dtx. Returns true if the callback
        ///           operation was successful. Returning false halts further
        ///           execution of the dtx and sets the dtx state to failed.
        void set_prepare_cb(const prepare_cb_t& cb);

        /// Registers a callback to be called before starting the commit phase
        /// of the dtx
        /// \param cb callback function taking the dtx ID, a vector of flags
        ///           indicating which transactions to complete, and a vector
        ///           of vectors indicating the indexes for each shard included
        ///           in the complete transactions vector. Returns true if the
        ///           callback operation was successful. Returning false halts
        ///           further execution of the dtx and sets the dtx state to
        ///           failed.
        void set_commit_cb(const commit_cb_t& cb);

        /// Registers a callback to be called before the discard phase of the
        /// dtx is started
        /// \param cb callback function taking the dtx ID as argument and
        ///           returning true if the callback operation was successful.
        ///           Returning false halts further execution of the dtx and
        ///           sets the dtx state to failed.
        void set_discard_cb(const discard_cb_t& cb);

        /// Registers a callback to be called before the done phase of the dtx
        /// is started
        /// \param cb callback function taking the dtx ID as argument and
        ///           returning true if the callback operation was successful.
        ///           Returning false halts further execution of the dtx and
        ///           sets the dtx state to failed.
        void set_done_cb(const done_cb_t& cb);

        /// Sets the state of the dtx to prepare and re-adds all the txs
        /// included in the batch
        /// \param txs list of txs included in the dtx batch
        void recover_prepare(const std::vector<transaction::compact_tx>& txs);

        /// Sets the state of the dtx to commit and sets the state from the end
        /// of the prepare phase so that execute() will continue the commit
        /// phase
        /// \param complete_txs vector of flags indicating which transactions
        ///                     in the batch to complete or cancel. The return
        ///                     value from the prepare phase.
        /// \param tx_idxs vector for each shard indicating which txs in the
        ///                complete_txs vector apply to each shard. Generated
        ///                before the prepare phase.
        void recover_commit(const std::vector<bool>& complete_txs,
                            const std::vector<std::vector<uint64_t>>& tx_idxs);

        /// Sets the state of the dtx to discard so that execute() will start
        /// from the discard phase
        void recover_discard();

        /// Returns the number of transactions in the dtx
        /// \return number of transactions in the batch
        [[nodiscard]] auto size() const -> size_t;

        enum class dtx_state {
            /// dtx initial state, no action has been performed yet
            start,
            /// dtx is calling prepare on shards
            prepare,
            /// dtx is calling commit on shards
            commit,
            /// dtx is calling discard on shards
            discard,
            /// dtx has completed fully
            done,
            /// dtx was interrupted and needs recovery. Shards will be left
            /// somewhere between states. For example, if the prepare phase has
            /// completed and the coordinator has started the commit phase,
            /// some shards will be committed and others could still be in the
            /// prepare phase. The dtx will need to be recovered from the start
            /// of the commit phase to ensure all shards are committed.
            failed
        };

        /// Returns the current state of the dtx
        /// \return state of the dtx
        [[nodiscard]] auto get_state() const -> dtx_state;

      private:
        [[nodiscard]] auto prepare() -> std::optional<std::vector<bool>>;

        [[nodiscard]] auto commit(const std::vector<bool>& complete_txs)
            -> bool;

        auto discard() -> bool;

        hash_t m_dtx_id;
        std::vector<std::shared_ptr<locking_shard::interface>> m_shards;
        std::vector<std::vector<locking_shard::tx>> m_txs;
        std::vector<transaction::compact_tx> m_full_txs;
        std::vector<std::vector<uint64_t>> m_tx_idxs;
        prepare_cb_t m_prepare_cb;
        commit_cb_t m_commit_cb;
        discard_cb_t m_discard_cb;
        done_cb_t m_done_cb;
        dtx_state m_state{dtx_state::start};
        std::vector<bool> m_complete_txs;
        std::shared_ptr<logging::log> m_logger;
    };
}

#endif // OPENCBDC_TX_SRC_COORDINATOR_DISTRIBUTED_TX_H_
