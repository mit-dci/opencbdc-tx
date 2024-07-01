// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_LOCKING_SHARD_LOCKING_SHARD_H_
#define OPENCBDC_TX_SRC_LOCKING_SHARD_LOCKING_SHARD_H_

#include "client.hpp"
#include "interface.hpp"
#include "status_interface.hpp"
#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/transaction.hpp"
#include "util/common/cache_set.hpp"
#include "util/common/hash.hpp"
#include "util/common/hashmap.hpp"
#include "util/common/snapshot_map.hpp"
#include "util/common/logging.hpp"

#include <filesystem>
#include <future>
#include <leveldb/db.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace cbdc::locking_shard {
    /// \brief In-memory implementation of \ref interface and
    /// \ref status_interface.
    ///
    /// \warning Not thread safe.
    /// Implements a UHS through conservative two-phase locking. Callers
    /// atomically check a batch of prospective transactions for spendable
    /// input UHS IDs in this shard's range, and lock those UHS IDs. Based on
    /// confirming responses from other shards, callers specify which
    /// transactions to complete and which to abort. Shards use a unique batch
    /// ID to track each batch across lock and apply (completion) operations to
    /// allow for parallel batch processing. The completed transactions cache
    /// allows users to determine whether a given transaction ID has been
    /// recently applied in the system. This is useful for recipients in a
    /// transaction to verify that the transaction has completed, or if the
    /// sender disconnects from the sentinel before receiving a response.
    class locking_shard final : public interface, public status_interface {
      public:
        /// Constructor.
        /// \param output_range inclusive range of hash prefixes this shard is
        ///                     responsible for.
        /// \param logger log instance.
        /// \param completed_txs_cache_size number of confirmed TX IDs to keep
        ///                                 before evicting the oldest TX ID.
        /// \param preseed_file path to file containing shard pre-seeding data
        ///                     or empty string to disable pre-seeding.
        /// \param opts configuration options.
        locking_shard(const std::pair<uint8_t, uint8_t>& output_range,
                      std::shared_ptr<logging::log> logger,
                      size_t completed_txs_cache_size,
                      const std::string& preseed_file,
                      config::options opts);
        locking_shard() = delete;

        /// \brief Attempts to lock the input hashes for the given batch of
        /// transactions.
        ///
        /// Only considers input hashes within this shard's range. The batch of
        /// transactions is a single distributed transaction, or 'dtx'. The
        /// coordinator that communicates with instances of this class provides
        /// a globally unique dtx ID along for each dtx. The provided vector of
        /// transactions may be a subset of the overall
        /// batch only including transactions relevant to this shard.
        /// \param txs list of txs to attempt to lock.
        /// \param dtx_id distributed tx ID for lock operation.
        /// \return if lock succeeds, return a vector of flags corresponding to
        ///         the txs in the input which had their relevant input hashes
        ///         locked by the shard. Otherwise std::nullopt.
        auto lock_outputs(std::vector<tx>&& txs, const hash_t& dtx_id)
            -> std::optional<std::vector<bool>> final;

        /// \brief Selectively applies the transactions from a previous lock
        /// operation.
        ///
        /// Follows confirmation from other shards via the coordinator.
        /// Completes a previous lock operation by deleting the input hashes
        /// and adding the output hashes of confirmed transactions, and
        /// unlocking the input hashes of aborted transactions. Preceded by a
        /// lock operation with the same dtx_id.
        /// \param complete_txs vector of truth values indicating which txs
        ///                     from the previous lock operation the shard
        ///                     should apply (`true`), and which it should
        ///                     cancel (`false`). Must have the same size and
        ///                      order as txs from lock.
        /// \param dtx_id distributed transaction ID of the previous lock
        ///               operation.
        /// \return true if the apply operation succeeded.
        auto apply_outputs(std::vector<bool>&& complete_txs,
                           const hash_t& dtx_id) -> bool final;

        /// Discards any cached information about a given distributed
        /// transaction. Called as the final step of a distributed transaction
        /// once all other participating shards have finished processing \ref
        /// apply_outputs.
        /// \param dtx_id distributed transaction ID of a previous apply
        ///               command.
        /// \return true if the discard operation succeeded.
        auto discard_dtx(const hash_t& dtx_id) -> bool final;

        /// \brief Stops the locking shard from processing further commands.
        /// Any future calls to methods of this class will return a failure
        /// result.
        void stop() final;

        /// Queries whether the shard's UHS contains the given UHS ID.
        /// \param uhs_id UHS ID to query.
        /// \return true if the UHS ID is unspent, false if not. std::nullopt
        ///         if the query failed.
        [[nodiscard]] auto check_unspent(const hash_t& uhs_id)
            -> std::optional<bool> final;

        /// Queries whether the given TX ID is confirmed in the cache of
        /// recently confirmed TX IDs.
        /// \param tx_id TX ID to query.
        /// \return true if cache contains TX ID, or false if not. std::nullopt
        ///         if the query failed.
        [[nodiscard]] auto check_tx_id(const hash_t& tx_id)
            -> std::optional<bool> final;

        /// Takes a snapshot of the UHS and calculates the supply of coins at
        /// the given epoch. Checks the UHS IDs match the value and nested data
        /// included in the UHS element.
        /// \param epoch the epoch to audit the supply at.
        /// \return commitment to total value in this shard's UHS. std::nullopt
        ///         if any of the UTXOs do not match their UHS ID.
        auto get_summary(uint64_t epoch) -> std::optional<commitment_t>;

        /// Prunes any spent UHS elements spent prior to the given epoch.
        /// \param epoch epoch to prune prior to.
        void prune(uint64_t epoch);

        /// Returns the highest epoch seen by the shard so far.
        /// \return highest epoch.
        auto highest_epoch() const -> uint64_t;

        struct uhs_element {
            /// UTXO
            transaction::compact_output m_out{};
            /// Epoch in which the UTXO was created.
            uint64_t m_creation_epoch{};
            /// Epoch in which the UTXO was spent, or std::nullopt if
            /// it is unspent.
            std::optional<uint64_t> m_deletion_epoch{};
        };

      private:
        auto read_preseed_file(const std::string& preseed_file) -> bool;
        auto check_and_lock_tx(const tx& t) -> bool;
        void apply_tx(const tx& t, bool complete);

        struct prepared_dtx {
            std::vector<tx> m_txs;
            std::vector<bool> m_results;
        };
        std::atomic_bool m_running{true};

        std::shared_ptr<logging::log> m_logger;
        mutable std::shared_mutex m_mut;
        snapshot_map<hash_t, uhs_element> m_uhs;
        snapshot_map<hash_t, uhs_element> m_locked;
        snapshot_map<hash_t, uhs_element> m_spent;
        std::optional<rangeproof_t> m_seed_rangeproof{};
        std::unordered_map<hash_t, prepared_dtx, hashing::null>
            m_prepared_dtxs;
        std::unordered_set<hash_t, hashing::null> m_applied_dtxs;
        cbdc::cache_set<hash_t, hashing::null> m_completed_txs;
        config::options m_opts;
        uint64_t m_highest_epoch{};

        using secp256k1_context_destroy_type = void (*)(secp256k1_context*);

        std::unique_ptr<secp256k1_context,
                        secp256k1_context_destroy_type>
            m_secp{secp256k1_context_create(SECP256K1_CONTEXT_NONE),
                   &secp256k1_context_destroy};

        static const inline auto m_random_source
            = std::make_unique<random_source>(config::random_source);

        struct GensDeleter {
            explicit GensDeleter(secp256k1_context* ctx) : m_ctx(ctx) {}

            void operator()(secp256k1_bppp_generators* gens) const {
                secp256k1_bppp_generators_destroy(m_ctx, gens);
            }

            secp256k1_context* m_ctx;
        };

        /// should be set to exactly `floor(log_base(value)) + 1`
        ///
        /// We use n_bits = 64, base = 16, so this should always be 24.
        static const inline auto generator_count = 16 + 8;

        std::unique_ptr<secp256k1_bppp_generators, GensDeleter>
            m_generators{
                secp256k1_bppp_generators_create(m_secp.get(),
                                                 generator_count),
                GensDeleter(m_secp.get())};
    };
}

#endif // OPENCBDC_TX_SRC_LOCKING_SHARD_LOCKING_SHARD_H_
