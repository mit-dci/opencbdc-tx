// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "messages.hpp"

namespace cbdc::atomizer {
    auto
    tx_notify_request::operator==(const tx_notify_request& rhs) const -> bool {
        return (rhs.m_tx == m_tx) && (rhs.m_attestations == m_attestations)
            && (rhs.m_block_height == m_block_height);
    }

    auto aggregate_tx_notification::operator==(
        const aggregate_tx_notification& rhs) const -> bool {
        return (rhs.m_oldest_attestation == m_oldest_attestation)
            && (rhs.m_tx == m_tx);
    }

    auto aggregate_tx_notify_request::operator==(
        const aggregate_tx_notify_request& rhs) const -> bool {
        return rhs.m_agg_txs == m_agg_txs;
    }
}
