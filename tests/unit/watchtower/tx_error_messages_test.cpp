// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "watchtower/tx_error_messages.hpp"

#include <gtest/gtest.h>

class tx_error_messages_test : public ::testing::Test {};

TEST_F(tx_error_messages_test, different_error_not_equal) {
    auto err0 = cbdc::watchtower::tx_error{
        {'a'},
        cbdc::watchtower::tx_error_inputs_dne{{{'b'}}}};
    auto err1
        = cbdc::watchtower::tx_error{{'a'}, cbdc::watchtower::tx_error_sync{}};
    ASSERT_FALSE(err0 == err1);
}

TEST_F(tx_error_messages_test, same_error_equal) {
    auto err0
        = cbdc::watchtower::tx_error{{'a'}, cbdc::watchtower::tx_error_sync{}};
    auto err1
        = cbdc::watchtower::tx_error{{'a'}, cbdc::watchtower::tx_error_sync{}};
    ASSERT_EQ(err0, err1);
}
