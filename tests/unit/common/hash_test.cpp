// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "common/hash.hpp"

#include <gtest/gtest.h>

class hash_test : public ::testing::Test {
  protected:
    cbdc::hash_t m_hash{0, 1, 2, 3, 4, 5, 255};
    std::string m_str{
        "000102030405ff00000000000000000000000000000000000000000000000000"};
};

TEST_F(hash_test, to_string) {
    auto act_val = cbdc::to_string(m_hash);
    EXPECT_EQ(m_str, act_val);
}

TEST_F(hash_test, hash_from_hex) {
    auto act_val = cbdc::hash_from_hex(m_str);
    EXPECT_EQ(m_hash, act_val);
}
