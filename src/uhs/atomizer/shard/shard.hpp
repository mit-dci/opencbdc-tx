// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/** \file shard.hpp
 * Shard core functionality.
 */

#ifndef OPENCBDC_TX_SRC_SHARD_SHARD_H_
#define OPENCBDC_TX_SRC_SHARD_SHARD_H_

#include "uhs/atomizer/atomizer/atomizer_raft.hpp"
#include "uhs/atomizer/atomizer/block.hpp"
#include "uhs/atomizer/atomizer/format.hpp"
#include "uhs/atomizer/watchtower/tx_error_messages.hpp"
#include "uhs/transaction/transaction.hpp"
#include "util/common/config.hpp"
#include "util/common/logging.hpp"
#include "util/network/connection_manager.hpp"
#include "util/serialization/format.hpp"

#include <atomic>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace cbdc::shard {
    /// Database shard representing a fraction of the UTXO set. Receives
    /// transactions from sentinels, and generates transaction input validity
    /// attestations to forward to the atomizer. Receives confirmed transaction
    /// blocks from the atomizer to update its internal state.
    class shard {
      public:
        /// Constructor. Call open_db() before using.
        /// \param prefix_range the inclusive UHS ID prefix range which this shard should track.
        explicit shard(config::shard_range_t prefix_range);

        /// Creates or restores this shard's UTXO database.
        /// \param db_dir relative path to the directory to create or read this shard's database files.
        /// \return nullopt if the shard successfully opened the database. Otherwise, returns the error message.
        auto open_db(const std::string& db_dir) -> std::optional<std::string>;

        /// Checks the validity of a provided transaction's inputs, and returns
        /// a transaction notification to forward to the atomizer or a
        /// transaction error to forward to the watchtower.
        /// \param tx the transaction to digest.
        /// \return result message to forward.
        auto digest_transaction(transaction::compact_tx tx)
            -> std::variant<atomizer::tx_notify_request, watchtower::tx_error>;

        /// Updates records to reflect changes from a new, contiguous
        /// transaction block from the atomizer. Deletes spent UTXOs and adds
        /// new ones. Increments the best block height. Accepts only blocks
        /// whose block height is one greater than the previous best block
        /// height; rejects non-contiguous blocks.
        /// \param blk the block to digest.
        /// \return true if the shard successfully digested the block. False if the block height is not contiguous.
        auto digest_block(const cbdc::atomizer::block& blk) -> bool;

        /// Returns the height of the most recently digested block.
        /// \return the best block height.
        [[nodiscard]] auto best_block_height() const -> uint64_t;

      private:
        [[nodiscard]] auto
        is_output_on_shard(const hash_t& uhs_hash) const -> bool;

        void update_snapshot();

        std::unique_ptr<leveldb::DB> m_db;
        leveldb::ReadOptions m_read_options;
        leveldb::WriteOptions m_write_options;

        uint64_t m_best_block_height{};

        std::shared_ptr<const leveldb::Snapshot> m_snp;
        uint64_t m_snp_height{};
        std::shared_mutex m_snp_mut;

        const std::string m_best_block_height_key = "bestBlockHeight";

        std::pair<uint8_t, uint8_t> m_prefix_range;
    };
}

#endif // OPENCBDC_TX_SRC_SHARD_SHARD_H_
