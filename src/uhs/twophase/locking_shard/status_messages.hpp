// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_LOCKING_SHARD_STATUS_MESSAGES_H_
#define OPENCBDC_TX_SRC_LOCKING_SHARD_STATUS_MESSAGES_H_

#include "util/common/hash.hpp"

#include <variant>

namespace cbdc::locking_shard::rpc {
    /// RPC message for clients to use to request the status of a UHS ID.
    struct uhs_status_request {
        /// UHS ID to check.
        hash_t m_uhs_id{};
    };

    /// RPC message for clients to use to request the status of a TX ID.
    struct tx_status_request {
        /// TX ID to check.
        hash_t m_tx_id{};
    };

    /// Status request RPC message wrapper, holding either a UHS ID or TX ID
    /// query request.
    using status_request = std::variant<uhs_status_request, tx_status_request>;

    /// Status response RPC messages indicating whether the shard contains
    /// given UHS or TX ID.
    using status_response = bool;
}

#endif
