// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/** \file status_update_messages.hpp
 * Watchtower module to process and cache block history.
 */

#ifndef OPENCBDC_TX_SRC_WATCHTOWER_BLOCK_CACHE_H_
#define OPENCBDC_TX_SRC_WATCHTOWER_BLOCK_CACHE_H_

#include "uhs/atomizer/atomizer/block.hpp"
#include "util/common/hashmap.hpp"

#include <forward_list>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <unordered_map>

namespace cbdc::watchtower {
    /// With respect to a particular UHS ID, block height + ID of containing
    /// transaction.
    using block_cache_result = std::pair<size_t, hash_t>;
    /// Stores a set of blocks in memory and maintains an index of the UHS IDs
    /// contained therein.
    class block_cache {
      public:
        block_cache() = delete;

        /// Constructor.
        /// \param k number of blocks to store in memory. 0 -> no limit.
        explicit block_cache(size_t k);

        /// Moves a block into the block cache, evicting the oldest block if
        /// the cache has reached its maximum size.
        /// \param blk the block to move into the cache.
        void push_block(cbdc::atomizer::block&& blk);

        /// Checks to see if the given UHS ID is spendable according to the
        /// blocks in the cache.
        /// \param uhs_id UHS ID to check.
        /// \return a BlockCacheResult containing the block height and ID of the transaction that originated this UHS ID.
        auto check_unspent(const hash_t& uhs_id) const
            -> std::optional<block_cache_result>;

        /// Checks to see if the given UHS ID has been spent according to the
        /// blocks in the cache.
        /// \param uhs_id UHS ID to check.
        /// \return a BlockCacheResult containing the block height and ID of the transaction that spent this UHS ID.
        auto check_spent(const hash_t& uhs_id) const
            -> std::optional<block_cache_result>;

        /// Returns the block height of the highest observed block.
        /// \return the highest block height.
        auto best_block_height() const -> uint64_t;

      private:
        size_t m_k_blks;
        std::queue<cbdc::atomizer::block> m_blks;
        uint64_t m_best_blk_height{0};
        std::unordered_map<hash_t,
                           block_cache_result,
                           hashing::const_sip_hash<hash_t>>
            m_unspent_ids;
        std::unordered_map<hash_t,
                           block_cache_result,
                           hashing::const_sip_hash<hash_t>>
            m_spent_ids;
    };
}

#endif // OPENCBDC_TX_SRC_WATCHTOWER_BLOCK_CACHE_H_
