// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_AGENT_EVM_MATH_H_
#define OPENCBDC_TX_SRC_3PC_AGENT_EVM_MATH_H_

#include <evmc/evmc.hpp>

namespace cbdc::threepc::agent::runner {
    /// Adds two uint256be values.
    /// \param lhs first value.
    /// \param rhs second value.
    /// \return sum of both values.
    auto operator+(const evmc::uint256be& lhs, const evmc::uint256be& rhs)
        -> evmc::uint256be;

    /// Subtracts two uint256be values.
    /// \param lhs value to subtract from.
    /// \param rhs value to subtract.
    /// \return lhs - rhs
    auto operator-(const evmc::uint256be& lhs, const evmc::uint256be& rhs)
        -> evmc::uint256be;

    /// Multiplies two uint256be values
    /// \param lhs first value.
    /// \param rhs second value.
    /// \return lhs * rhs
    auto operator*(const evmc::uint256be& lhs, const evmc::uint256be& rhs)
        -> evmc::uint256be;

    /// Left shifts a uint256be value by a given number of bytes.
    /// \param lhs value to shift.
    /// \param count number of bytes to shift by.
    /// \return lhs left shifted by count bytes.
    auto operator<<(const evmc::uint256be& lhs, size_t count)
        -> evmc::uint256be;
}

#endif
