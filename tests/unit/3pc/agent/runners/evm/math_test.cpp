// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "3pc/agent/runners/evm/math.hpp"

#include <evmc/evmc.hpp>
#include <gtest/gtest.h>

class math_test : public ::testing::Test {};

using namespace cbdc::threepc::agent::runner;

TEST_F(math_test, addition) {
    auto val1 = evmc::uint256be(1000);
    auto val2 = evmc::uint256be(1500);
    auto res = val1 + val2;
    auto exp = evmc::uint256be(2500);
    ASSERT_EQ(res, exp);
}

TEST_F(math_test, multiplication) {
    auto val1 = evmc::uint256be(1000);
    auto val2 = evmc::uint256be(1500);
    auto res = val1 * val2;
    auto exp = evmc::uint256be(1500000);
    ASSERT_EQ(res, exp);
}

TEST_F(math_test, subtraction) {
    auto val1 = evmc::uint256be(1000);
    auto val2 = evmc::uint256be(1500);
    auto res = val2 - val1;
    auto exp = evmc::uint256be(500);
    ASSERT_EQ(res, exp);
}

TEST_F(math_test, lshift) {
    auto val1 = evmc::uint256be(20000000);
    auto res = val1 << 2;
    auto exp = evmc::uint256be(1310720000000);
    ASSERT_EQ(res, exp);
}
