// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "format.hpp"

#include "uhs/transaction/messages.hpp"
#include "util/serialization/format.hpp"

namespace cbdc {
    auto operator<<(serializer& packet, const locking_shard::tx& tx)
        -> serializer& {
        return packet << tx.m_tx << tx.m_epoch;
    }

    auto operator>>(serializer& packet, locking_shard::tx& tx) -> serializer& {
        return packet >> tx.m_tx >> tx.m_epoch;
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

    auto operator<<(serializer& ser,
                    const locking_shard::locking_shard::uhs_element& p)
        -> serializer& {
        return ser << p.m_out << p.m_creation_epoch
                   << p.m_deletion_epoch;
    }

    auto operator>>(serializer& deser,
                    locking_shard::locking_shard::uhs_element& p)
        -> serializer& {
        return deser >> p.m_out >> p.m_creation_epoch
                     >> p.m_deletion_epoch;
    }
}
