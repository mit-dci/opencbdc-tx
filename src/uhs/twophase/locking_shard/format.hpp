// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_LOCKING_SHARD_FORMAT_H_
#define OPENCBDC_TX_SRC_LOCKING_SHARD_FORMAT_H_

#include "locking_shard.hpp"
#include "status_client.hpp"

namespace cbdc {
    auto operator<<(serializer& packet, const locking_shard::tx& tx)
        -> serializer&;
    auto operator>>(serializer& packet, locking_shard::tx& tx) -> serializer&;

    auto operator<<(serializer& packet, const locking_shard::rpc::request& p)
        -> serializer&;
    auto operator>>(serializer& packet, locking_shard::rpc::request& p)
        -> serializer&;

    auto operator<<(serializer& packet,
                    const locking_shard::rpc::tx_status_request& p)
        -> serializer&;
    auto operator>>(serializer& packet,
                    locking_shard::rpc::tx_status_request& p) -> serializer&;

    auto operator<<(serializer& packet,
                    const locking_shard::rpc::uhs_status_request& p)
        -> serializer&;
    auto operator>>(serializer& packet,
                    locking_shard::rpc::uhs_status_request& p) -> serializer&;
}

#endif // OPENCBDC_TX_SRC_LOCKING_SHARD_MESSAGES_H_
