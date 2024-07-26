// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_ATOMIZER_FORMAT_H_
#define OPENCBDC_TX_SRC_ATOMIZER_FORMAT_H_

#include "atomizer_raft.hpp"
#include "state_machine.hpp"

namespace cbdc {
    auto
    operator<<(serializer& ser,
               const atomizer::state_machine::snapshot& snp) -> serializer&;
    auto operator>>(serializer& deser,
                    atomizer::state_machine::snapshot& snp) -> serializer&;

    auto
    operator<<(serializer& packet,
               const atomizer::aggregate_tx_notification& msg) -> serializer&;
    auto operator>>(serializer& packet,
                    atomizer::aggregate_tx_notification& msg) -> serializer&;

    auto operator<<(serializer& packet,
                    const atomizer::aggregate_tx_notify_request& msg)
        -> serializer&;
    auto operator>>(serializer& packet,
                    atomizer::aggregate_tx_notify_request& msg) -> serializer&;

    auto operator<<(serializer& packet,
                    const atomizer::tx_notify_request& msg) -> serializer&;
    auto operator>>(serializer& packet,
                    atomizer::tx_notify_request& msg) -> serializer&;

    auto operator<<(serializer& packet,
                    const cbdc::atomizer::block& blk) -> serializer&;
    auto operator>>(serializer& packet,
                    cbdc::atomizer::block& blk) -> serializer&;

    auto operator<<(serializer& ser,
                    const atomizer::prune_request& r) -> serializer&;
    auto operator>>(serializer& deser,
                    atomizer::prune_request& r) -> serializer&;

    auto operator<<(serializer& ser,
                    const atomizer::make_block_request& r) -> serializer&;
    auto operator>>(serializer& deser,
                    atomizer::make_block_request& r) -> serializer&;

    auto operator<<(serializer& ser,
                    const atomizer::get_block_request& r) -> serializer&;
    auto operator>>(serializer& deser,
                    atomizer::get_block_request& r) -> serializer&;

    auto operator<<(serializer& ser,
                    const atomizer::make_block_response& r) -> serializer&;
    auto operator>>(serializer& deser,
                    atomizer::make_block_response& r) -> serializer&;

    auto operator<<(serializer& ser,
                    const atomizer::get_block_response& r) -> serializer&;
    auto operator>>(serializer& deser,
                    atomizer::get_block_response& r) -> serializer&;
}

#endif // OPENCBDC_TX_SRC_ATOMIZER_FORMAT_H_
