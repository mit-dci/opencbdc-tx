// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_AGENT_RUNNERS_EVM_FORMAT_H_
#define OPENCBDC_TX_SRC_PARSEC_AGENT_RUNNERS_EVM_FORMAT_H_

#include "messages.hpp"
#include "util/serialization/serializer.hpp"

namespace cbdc {
    auto operator<<(serializer& ser,
                    const parsec::agent::runner::evm_account& acc)
        -> serializer&;
    auto operator>>(serializer& deser, parsec::agent::runner::evm_account& acc)
        -> serializer&;

    auto operator<<(serializer& ser, const evmc::address& addr) -> serializer&;
    auto operator>>(serializer& deser, evmc::address& addr) -> serializer&;

    auto operator<<(serializer& ser, const evmc::bytes32& b) -> serializer&;
    auto operator>>(serializer& deser, evmc::bytes32& b) -> serializer&;

    auto operator<<(serializer& ser, const parsec::agent::runner::evm_tx& tx)
        -> serializer&;
    auto operator>>(serializer& deser, parsec::agent::runner::evm_tx& tx)
        -> serializer&;

    auto operator<<(serializer& ser, const parsec::agent::runner::evm_sig& s)
        -> serializer&;
    auto operator>>(serializer& deser, parsec::agent::runner::evm_sig& s)
        -> serializer&;

    auto operator<<(serializer& ser,
                    const parsec::agent::runner::evm_access_tuple& at)
        -> serializer&;
    auto operator>>(serializer& deser,
                    parsec::agent::runner::evm_access_tuple& at)
        -> serializer&;

    auto operator<<(serializer& ser, const parsec::agent::runner::evm_log& l)
        -> serializer&;
    auto operator>>(serializer& deser, parsec::agent::runner::evm_log& l)
        -> serializer&;

    auto operator<<(serializer& ser,
                    const parsec::agent::runner::evm_tx_receipt& r)
        -> serializer&;
    auto operator>>(serializer& deser,
                    parsec::agent::runner::evm_tx_receipt& r) -> serializer&;

    auto operator<<(serializer& ser, const parsec::agent::runner::code_key& k)
        -> serializer&;
    auto operator>>(serializer& deser, parsec::agent::runner::code_key& k)
        -> serializer&;

    auto operator<<(serializer& ser,
                    const parsec::agent::runner::storage_key& k)
        -> serializer&;
    auto operator>>(serializer& deser, parsec::agent::runner::storage_key& k)
        -> serializer&;
    auto operator<<(serializer& ser,
                    const parsec::agent::runner::evm_dryrun_tx& tx)
        -> serializer&;

    auto operator>>(serializer& deser,
                    parsec::agent::runner::evm_dryrun_tx& tx) -> serializer&;

    auto operator>>(serializer& deser,
                    parsec::agent::runner::evm_pretend_block& b)
        -> serializer&;
    auto operator<<(serializer& ser,
                    const parsec::agent::runner::evm_pretend_block& b)
        -> serializer&;

    auto operator>>(serializer& deser,
                    parsec::agent::runner::evm_log_query& lq) -> serializer&;
    auto operator<<(serializer& ser,
                    const parsec::agent::runner::evm_log_query& lq)
        -> serializer&;

    auto operator>>(serializer& deser,
                    parsec::agent::runner::evm_log_index& idx) -> serializer&;
    auto operator<<(serializer& ser,
                    const parsec::agent::runner::evm_log_index& idx)
        -> serializer&;
}

#endif
