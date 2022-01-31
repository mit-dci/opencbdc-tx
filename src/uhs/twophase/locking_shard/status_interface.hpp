// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_LOCKING_SHARD_STATUS_INTERFACE_H_
#define OPENCBDC_TX_SRC_LOCKING_SHARD_STATUS_INTERFACE_H_

#include "util/common/hash.hpp"

#include <optional>
#include <variant>

namespace cbdc::locking_shard {
    /// Interface for querying the read-only state of a locking shard. Returns
    /// whether a given UHS ID is currently unspent or a TX ID has been
    /// confirmed.
    class status_interface {
      public:
        status_interface() = default;
        virtual ~status_interface() = default;
        status_interface(const status_interface&) = default;
        auto operator=(const status_interface&) -> status_interface& = default;
        status_interface(status_interface&&) = default;
        auto operator=(status_interface&&) -> status_interface& = default;

        /// Queries whether the shard's UHS contains the given UHS ID.
        /// \param uhs_id UHS ID to query.
        /// \return true if the UHS ID is unspent, or std::nullopt if the query
        ///         failed.
        [[nodiscard]] virtual auto check_unspent(const hash_t& uhs_id)
            -> std::optional<bool> = 0;

        /// Queries whether the given TX ID is confirmed in the cache of
        /// recently confirmed TX IDs.
        /// \param tx_id TX ID to query.
        /// \return true if the TX ID is present in the cache, or std::nullopt
        ///         if the query failed.
        [[nodiscard]] virtual auto check_tx_id(const hash_t& tx_id)
            -> std::optional<bool> = 0;
    };
}

#endif
