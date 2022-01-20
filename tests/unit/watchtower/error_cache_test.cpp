// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "watchtower/error_cache.hpp"
#include "watchtower/tx_error_messages.hpp"

#include <gtest/gtest.h>

class error_cache_test : public ::testing::Test {
  protected:
    void SetUp() override {
        std::vector<cbdc::watchtower::tx_error> errs{
            {cbdc::watchtower::tx_error{
                 {'t', 'x', 'a'},
                 cbdc::watchtower::tx_error_inputs_dne{
                     {{'u', 'h', 's', 'a'}, {'u', 'h', 's', 'b'}}}},
             cbdc::watchtower::tx_error{
                 {'t', 'x', 'b'},
                 cbdc::watchtower::tx_error_stxo_range{}},
             cbdc::watchtower::tx_error{{'t', 'x', 'c'},
                                        cbdc::watchtower::tx_error_sync{}},
             cbdc::watchtower::tx_error{
                 {'t', 'x', 'd'},
                 cbdc::watchtower::tx_error_inputs_spent{
                     {{'u', 'h', 's', 'c'}, {'u', 'h', 's', 'd'}}}}}};
        m_ec.push_errors(std::move(errs));
    }

    cbdc::watchtower::error_cache m_ec{4};
};

TEST_F(error_cache_test, no_errors) {
    ASSERT_FALSE(m_ec.check_tx_id({'Z'}).has_value());
    ASSERT_FALSE(m_ec.check_uhs_id({'Z'}).has_value());
}

TEST_F(error_cache_test, add_k_plus_1) {
    ASSERT_TRUE(m_ec.check_tx_id({'t', 'x', 'a'}).has_value());
    ASSERT_TRUE(m_ec.check_uhs_id({'u', 'h', 's', 'a'}).has_value());
    ASSERT_TRUE(m_ec.check_uhs_id({'u', 'h', 's', 'b'}).has_value());
    ASSERT_TRUE(m_ec.check_tx_id({'t', 'x', 'b'}).has_value());
    ASSERT_TRUE(m_ec.check_tx_id({'t', 'x', 'c'}).has_value());
    ASSERT_TRUE(m_ec.check_tx_id({'t', 'x', 'd'}).has_value());
    ASSERT_TRUE(m_ec.check_uhs_id({'u', 'h', 's', 'c'}).has_value());
    ASSERT_TRUE(m_ec.check_uhs_id({'u', 'h', 's', 'd'}).has_value());

    m_ec.push_errors({cbdc::watchtower::tx_error{
        {'t', 'x', 'e'},
        cbdc::watchtower::tx_error_inputs_spent{
            {{'u', 'h', 's', 'e'}, {'u', 'h', 's', 'f'}}}}});

    ASSERT_TRUE(m_ec.check_tx_id({'t', 'x', 'e'}).has_value());
    ASSERT_TRUE(m_ec.check_uhs_id({'u', 'h', 's', 'e'}).has_value());
    ASSERT_TRUE(m_ec.check_uhs_id({'u', 'h', 's', 'f'}).has_value());
    ASSERT_TRUE(m_ec.check_tx_id({'t', 'x', 'b'}).has_value());
    ASSERT_TRUE(m_ec.check_tx_id({'t', 'x', 'c'}).has_value());
    ASSERT_TRUE(m_ec.check_tx_id({'t', 'x', 'd'}).has_value());
    ASSERT_TRUE(m_ec.check_uhs_id({'u', 'h', 's', 'c'}).has_value());
    ASSERT_TRUE(m_ec.check_uhs_id({'u', 'h', 's', 'd'}).has_value());

    ASSERT_FALSE(m_ec.check_tx_id({'t', 'x', 'a'}).has_value());
    ASSERT_FALSE(m_ec.check_uhs_id({'u', 'h', 's', 'a'}).has_value());
    ASSERT_FALSE(m_ec.check_uhs_id({'u', 'h', 's', 'b'}).has_value());
}
