// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_TOOLS_BENCH_PARSEC_EVM_CONTRACTS_H_
#define OPENCBDC_TX_TOOLS_BENCH_PARSEC_EVM_CONTRACTS_H_

#include "parsec/agent/runners/evm/hash.hpp"
#include "util/common/buffer.hpp"

#include <evmc/evmc.hpp>

namespace cbdc::parsec::evm_contracts {
    static constexpr size_t selector_size = 4;
    static constexpr size_t param_size = 32;

    static constexpr size_t address_param_offset
        = 12; // in ABIs addresses are also 32 bytes
    auto data_erc20_deploy() -> cbdc::buffer;

    auto data_erc20_allowance(evmc::address owner,
                              evmc::address spender) -> cbdc::buffer;

    auto data_erc20_approve(evmc::address spender,
                            evmc::uint256be amount) -> cbdc::buffer;

    auto data_erc20_balance_of(evmc::address account) -> cbdc::buffer;

    auto data_erc20_decimals() -> cbdc::buffer;

    auto data_erc20_decrease_allowance(evmc::address spender,
                                       evmc::uint256be subtracted_value)
        -> cbdc::buffer;

    auto
    data_erc20_increase_allowance(evmc::address spender,
                                  evmc::uint256be added_value) -> cbdc::buffer;

    auto data_erc20_name() -> cbdc::buffer;

    auto data_erc20_symbol() -> cbdc::buffer;

    auto data_erc20_total_supply() -> cbdc::buffer;

    auto data_erc20_transfer(evmc::address to,
                             evmc::uint256be amount) -> cbdc::buffer;

    auto data_erc20_transfer_from(evmc::address from,
                                  evmc::address to,
                                  evmc::uint256be amount) -> cbdc::buffer;

}

#endif // OPENCBDC_TX_TOOLS_BENCH_PARSEC_EVM_CONTRACTS_H_
