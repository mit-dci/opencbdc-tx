// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_LOCKING_SHARD_LOCKING_SHARD_INTERFACE_H_
#define OPENCBDC_TX_SRC_LOCKING_SHARD_LOCKING_SHARD_INTERFACE_H_

#include "uhs/transaction/transaction.hpp"
#include "util/common/hash.hpp"

#include <optional>
#include <variant>
#include <vector>

namespace cbdc::locking_shard {
    /// Transaction type processed by locking shards.
    struct tx {
        /// The TX ID of the transaction, if provided.
        std::optional<hash_t> m_tx_id{};
        /// Vector of input hashes for the shard to process as spent.
        std::vector<hash_t> m_spending{};
        /// Vector of output hashes to create on the shard.
        std::vector<transaction::compact_output> m_creating{};

        auto operator==(const tx& rhs) const -> bool;
    };

    /// Interface for a locking shard. Intended to allow for other
    /// classes to pick an implementation.
    /// \see locking_shard for an in-memory implementation using hashmaps
    /// \see client for an implementation that connects to a remote shard over
    ///      a network
    class interface {
      public:
        /// Constructor.
        /// \param output_range inclusive hash prefix range the shard is
        ///                     responsible for managing.
        explicit interface(std::pair<uint8_t, uint8_t> output_range);
        virtual ~interface() = default;
        interface() = delete;
        interface(const interface&) = delete;
        auto operator=(const interface&) -> interface& = delete;
        interface(interface&&) = delete;
        auto operator=(interface&&) -> interface& = delete;

        /// Attempts to lock the input hashes for the given vector of
        /// transactions. Only considers input hashes relevant to this shard
        /// based on the shard range. The batch of transactions is a single
        /// distributed transaction, or 'dtx', referred to by a globally unique
        /// dtx ID provided by the caller.
        /// \param txs list of txs to attempt to lock.
        /// \param dtx_id distributed tx ID for lock operation.
        /// \return if lock succeeds, return a vector of flags indicating which
        ///         txs in the input vector had their relevant input hashes
        ///         locked by the shard. Otherwise std::nullopt.
        virtual auto lock_outputs(std::vector<tx>&& txs, const hash_t& dtx_id)
            -> std::optional<std::vector<bool>> = 0;

        /// Completes a previous lock operation by deleting input hashes and
        /// creating output hashes, or unlocking input hashes.
        /// \param complete_txs vector of flags indicating which txs from the
        ///                     previous lock operation the shard should apply
        ///                     and which it should cancel. Must be the same
        ///                     size as txs from lock.
        /// \param dtx_id distributed transaction ID of the previous lock
        ///               operation.
        /// \return true if the apply operation succeeded.
        virtual auto apply_outputs(std::vector<bool>&& complete_txs,
                                   const hash_t& dtx_id) -> bool
            = 0;

        /// Returns whether a given hash is within the shard's range.
        /// \param h hash to check.
        /// \return true if the hash is within the shard's range.
        [[nodiscard]] virtual auto hash_in_shard_range(const hash_t& h) const
            -> bool;

        /// Discards any cached information about a given distributed
        /// transaction.
        /// \param dtx_id distributed transaction ID of a previous apply
        ///               command.
        /// \return true if the discard operation succeeded.
        virtual auto discard_dtx(const hash_t& dtx_id) -> bool = 0;

        /// Stops the locking shard implementation from processing further
        /// commands and unblocks any pending commands.
        virtual void stop() = 0;

      private:
        std::pair<uint8_t, uint8_t> m_output_range;
    };
}

#endif // OPENCBDC_TX_SRC_LOCKING_SHARD_LOCKING_SHARD_INTERFACE_H_
