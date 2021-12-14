// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "messages.hpp"

namespace cbdc::atomizer {
    auto tx_notify_request::operator==(const tx_notify_request& rhs) const
        -> bool {
        return (rhs.m_tx == m_tx) && (rhs.m_attestations == m_attestations)
            && (rhs.m_block_height == m_block_height);
    }

    auto aggregate_tx_notify::operator==(const aggregate_tx_notify& rhs) const
        -> bool {
        return (rhs.m_oldest_attestation == m_oldest_attestation)
            && (rhs.m_tx == m_tx);
    }

    auto aggregate_tx_notify_set::operator==(
        const aggregate_tx_notify_set& rhs) const -> bool {
        return (rhs.m_cmd == m_cmd) && (rhs.m_agg_txs == m_agg_txs);
    }
}
