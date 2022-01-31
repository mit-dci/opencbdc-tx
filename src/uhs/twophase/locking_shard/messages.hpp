// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_LOCKING_SHARD_MESSAGES_H_
#define OPENCBDC_TX_SRC_LOCKING_SHARD_MESSAGES_H_

#include "interface.hpp"

namespace cbdc::locking_shard::rpc {
    /// Transactions whose outputs the locking shard should lock
    using lock_params = std::vector<tx>;
    /// Vector of bools. True if the locking shard should complete the
    /// transaction at the same index in the previous batch. False if the
    /// locking shard should unlock the transaction.
    using apply_params = std::vector<bool>;
    /// Empty type for discard command parameters
    struct discard_params {
        constexpr auto operator==(const discard_params& /* rhs */) const
            -> bool {
            return true;
        };
    };

    /// Request to a shard
    struct request {
        /// The distributed transaction ID corresponding to the request
        hash_t m_dtx_id{};
        /// If the command is lock or apply, the parameters for these
        /// commands
        std::variant<lock_params, apply_params, discard_params> m_params{};

        auto operator==(const request& rhs) const -> bool;
    };

    /// Response from a lock command, a vector of flags indicating which
    /// transactions in the batch had their relevant inputs successfully
    /// locked
    using lock_response = std::vector<bool>;
    /// Empty type for the apply response
    struct apply_response {
        constexpr auto operator==(const apply_response& /* rhs */) const
            -> bool {
            return true;
        };
    };
    /// Empty type for the discard response
    struct discard_response {
        constexpr auto operator==(const discard_response& /* rhs */) const
            -> bool {
            return true;
        };
    };

    /// Response to a locking shard request
    using response
        = std::variant<lock_response, apply_response, discard_response>;
}

#endif
