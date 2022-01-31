// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_ATOMIZER_ATOMIZER_H_
#define OPENCBDC_TX_SRC_ATOMIZER_ATOMIZER_H_

#include "block.hpp"
#include "uhs/atomizer/watchtower/tx_error_messages.hpp"
#include "uhs/transaction/transaction.hpp"
#include "util/common/hashmap.hpp"

#include <map>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace cbdc::atomizer {
    /// \brief Atomizer implementation.
    ///
    /// Aggregates transaction notifications and input attestations. Accepts
    /// transactions with a full set of attestations, and provides block
    /// construction functionality. Keeps track of recently spent UHS IDs and
    /// the block height at which they were spent to enable input attestations
    /// that are not valid as of the most recent block height to still be used.
    /// This works because inputs covered by attesations with a block height
    /// lower than the most recent block will be in the spent cache if they are
    /// unspendable. Otherwise, the atomizer can be certain the inputs not have
    /// been spent.
    /// \warning Not thread-safe. Does not persist the atomizer internal state.
    class atomizer {
      public:
        /// Constructor.
        /// \param best_height starting block height.
        /// \param stxo_cache_depth maximum number of recent blocks over which
        ///                         to maintain the spent UHS IDs cache.
        atomizer(uint64_t best_height, size_t stxo_cache_depth);

        ~atomizer() = default;

        atomizer() = delete;
        atomizer(const atomizer&) = delete;
        auto operator=(const atomizer&) -> atomizer& = delete;
        atomizer(atomizer&&) = delete;
        auto operator=(atomizer&&) -> atomizer& = delete;

        /// Attempts to add the specified shard attestations for a specified
        /// transaction at or later than the specified block height. Creates a
        /// new pending transaction if necessary. If the provided transaction
        /// is already pending, merges the new attestations. Returns an error
        /// to forward to the watchtower if:
        /// - The block height is below the lower limit of the STXO cache.
        /// - The STXO cache contains one of the transaction's inputs (double
        ///   spend).
        /// \param block_height the block height at which the shard provided
        ///                     the attestations.
        /// \param tx the complete transaction.
        /// \param attestations a set of the input indices to which the shard
        ///                     attested the validity.
        /// \return an error to forward to the watchtower, if necessary.
        [[nodiscard]] auto insert(uint64_t block_height,
                                  transaction::compact_tx tx,
                                  std::unordered_set<uint32_t> attestations)
            -> std::optional<watchtower::tx_error>;

        /// \brief Attempts to add the given compact transaction to the list of
        /// complete transactions pending for inclusion in the next block.
        ///
        /// If the block height of the oldest attestion in the transaction
        /// precedes the height of the earliest block in the spent UHS ID
        /// cache, discards the transaction and returns a watchtower error. If
        /// the compact transaction attempts to spend a UHS ID matching one in
        /// the spent UHS ID cache, discards the compact TX and returns a
        /// watchtower error.
        /// \param oldest_attestation block height of the oldest shard
        ///                           attestation in the notification.
        /// \param tx compact transaction to insert in the next block.
        /// \return std::nullopt if the operation successfully inserted the
        ///         transaction into the next block, or the relevant watchtower
        ///         error on failure.
        [[nodiscard]] auto insert_complete(uint64_t oldest_attestation,
                                           transaction::compact_tx&& tx)
            -> std::optional<watchtower::tx_error>;

        /// Adds the current set of complete transactions to a new block and
        /// returns it for storage and transmission to subscribers. Rotates the
        /// STXO cache, evicting the oldest set of transactions. Generates and
        /// returns a set of errors containing an error for each incomplete
        /// transaction in the set of evicted transactions, or an empty vector
        /// if there are no such errors.
        /// \return a pair containing the resultant block and errors to forward
        ///         to the watchtower if necessary.
        [[nodiscard]] auto make_block()
            -> std::pair<cbdc::atomizer::block,
                         std::vector<watchtower::tx_error>>;

        /// Returns the number of complete transactions waiting to be
        /// included in the next block.
        /// \return number of transactions.
        [[nodiscard]] auto pending_transactions() const -> size_t;

        /// Returns the height of the most recent block.
        /// \return block height.
        [[nodiscard]] auto height() const -> uint64_t;

        /// Serializes the internal state of the atomizer into a buffer.
        /// \return serialized atomizer state.
        [[nodiscard]] auto serialize() -> buffer;

        /// Replaces the state of this atomizer instance with the provided
        /// serialized state data.
        /// \param buf serialized atomizer state produced with \ref serialize.
        void deserialize(serializer& buf);

        auto operator==(const atomizer& other) const -> bool;

      private:
        std::vector<std::unordered_map<transaction::compact_tx,
                                       std::unordered_set<uint32_t>,
                                       transaction::compact_tx_hasher>>
            m_txs;

        // These maps should be keyed/salted for safety. For now they
        // use input values directly as an optimization.
        std::vector<transaction::compact_tx> m_complete_txs;

        std::vector<std::unordered_set<hash_t, hashing::null>> m_spent;

        uint64_t m_best_height{};
        size_t m_spent_cache_depth;

        [[nodiscard]] auto get_notification_offset(uint64_t block_height) const
            -> uint64_t;

        [[nodiscard]] auto
        check_notification_offset(uint64_t height_offset,
                                  const transaction::compact_tx& tx) const
            -> std::optional<watchtower::tx_error>;

        [[nodiscard]] auto check_stxo_cache(const transaction::compact_tx& tx,
                                            uint64_t cache_check_range) const
            -> std::optional<watchtower::tx_error>;

        void add_tx_to_stxo_cache(const transaction::compact_tx& tx);
    };
}

#endif // OPENCBDC_TX_SRC_ATOMIZER_ATOMIZER_H_
