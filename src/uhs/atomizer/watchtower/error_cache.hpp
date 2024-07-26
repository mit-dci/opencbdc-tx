// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/** \file status_update_messages.hpp
 * Watchtower module to cache transaction errors.
 */

#ifndef OPENCBDC_TX_SRC_WATCHTOWER_ERROR_CACHE_H_
#define OPENCBDC_TX_SRC_WATCHTOWER_ERROR_CACHE_H_

#include "tx_error_messages.hpp"
#include "util/common/hashmap.hpp"

#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <unordered_map>

namespace cbdc::watchtower {

    /// Stores a set of internal transaction errors in memory, indexed by Tx ID
    /// and UHS ID.
    class error_cache {
      public:
        error_cache() = delete;

        /// Constructor.
        /// \param k number of errors to store in memory. 0 -> no limit.
        explicit error_cache(size_t k);

        /// Moves an error into the error cache, evicting the oldest error if
        /// the cache has reached its maximum size.
        /// \param errs the error to move.
        void push_errors(std::vector<tx_error>&& errs);

        /// Checks the cache for an error associated with the given Tx ID.
        /// \param tx_id the Tx ID to check.
        /// \return error information, or nullopt if not found.
        auto check_tx_id(const hash_t& tx_id) const -> std::optional<tx_error>;

        /// Checks the cache for an error associated with the given UHS ID.
        /// \param uhs_id the UHS ID to check.
        /// \return error information, or nullopt if not found.
        auto
        check_uhs_id(const hash_t& uhs_id) const -> std::optional<tx_error>;

      private:
        size_t m_k_errs;
        std::queue<std::shared_ptr<tx_error>> m_errs;
        std::unordered_map<hash_t,
                           std::shared_ptr<tx_error>,
                           hashing::const_sip_hash<hash_t>>
            m_uhs_errs;
        std::unordered_map<hash_t,
                           std::shared_ptr<tx_error>,
                           hashing::const_sip_hash<hash_t>>
            m_tx_id_errs;
    };
}

#endif // OPENCBDC_TX_SRC_WATCHTOWER_ERROR_CACHE_H_
