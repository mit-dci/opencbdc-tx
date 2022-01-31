// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "format.hpp"

#include "util/serialization/format.hpp"

namespace cbdc {
    auto operator<<(serializer& packet, const locking_shard::tx& tx)
        -> serializer& {
        return packet << tx.m_tx_id << tx.m_spending << tx.m_creating;
    }

    auto operator>>(serializer& packet, locking_shard::tx& tx) -> serializer& {
        return packet >> tx.m_tx_id >> tx.m_spending >> tx.m_creating;
    }

    auto operator<<(serializer& packet, const locking_shard::rpc::request& p)
        -> serializer& {
        return packet << p.m_dtx_id << p.m_params;
    }

    auto operator>>(serializer& packet, locking_shard::rpc::request& p)
        -> serializer& {
        return packet >> p.m_dtx_id >> p.m_params;
    }

    auto operator<<(serializer& packet,
                    const locking_shard::rpc::tx_status_request& p)
        -> serializer& {
        return packet << p.m_tx_id;
    }

    auto operator>>(serializer& packet,
                    locking_shard::rpc::tx_status_request& p) -> serializer& {
        return packet >> p.m_tx_id;
    }

    auto operator<<(serializer& packet,
                    const locking_shard::rpc::uhs_status_request& p)
        -> serializer& {
        return packet << p.m_uhs_id;
    }

    auto operator>>(serializer& packet,
                    locking_shard::rpc::uhs_status_request& p) -> serializer& {
        return packet >> p.m_uhs_id;
    }
}
