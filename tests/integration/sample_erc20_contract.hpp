// Copyright (c) 2023 MIT Digital Currency Initiative,
//
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TEST_INTEGRATION_SAMPLE_ERC20_CONTRACT_H_
#define OPENCBDC_TEST_INTEGRATION_SAMPLE_ERC20_CONTRACT_H_

#include "parsec/agent/runners/evm/hash.hpp"
#include "util/common/buffer.hpp"

#include <evmc/evmc.hpp>

/**
 * This sample EVM contract bytecode and ABI interface have been copied from
 * tools/bench/parsec/evm/contracts where the originating Solidity code looks
as
 * follows and was compiled using hardhat.

pragma solidity ^0.8.0;
import "@openzeppelin/contracts/token/ERC20/ERC20.sol";
contract Token is ERC20 {
    constructor() ERC20("Tokens", "TOK") {
        _mint(msg.sender, 1000000000000000000000000); // 1M Coins with 18
decimals
    }
}
 */
namespace cbdc::test::evm_contracts {
    static constexpr size_t selector_size = 4;
    static constexpr size_t param_size = 32;

    static constexpr size_t address_param_offset
        = 12; // in ABIs addresses are also 32 bytes
    auto data_erc20_contract_bytecode() -> cbdc::buffer;

    auto data_erc20_allowance(evmc::address owner, evmc::address spender)
        -> cbdc::buffer;

    auto data_erc20_approve(evmc::address spender, evmc::uint256be amount)
        -> cbdc::buffer;

    auto data_erc20_balance_of(evmc::address account) -> cbdc::buffer;

    auto data_erc20_decimals() -> cbdc::buffer;

    auto data_erc20_decrease_allowance(evmc::address spender,
                                       evmc::uint256be subtracted_value)
        -> cbdc::buffer;

    auto data_erc20_increase_allowance(evmc::address spender,
                                       evmc::uint256be added_value)
        -> cbdc::buffer;

    auto data_erc20_name() -> cbdc::buffer;

    auto data_erc20_symbol() -> cbdc::buffer;

    auto data_erc20_total_supply() -> cbdc::buffer;

    auto data_erc20_transfer(evmc::address to, evmc::uint256be amount)
        -> cbdc::buffer;

    auto data_erc20_transfer_from(evmc::address from,
                                  evmc::address to,
                                  evmc::uint256be amount) -> cbdc::buffer;

}

#endif // OPENCBDC_TEST_INTEGRATION_SAMPLE_ERC20_CONTRACT_H_
