// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/atomizer/watchtower/block_cache.hpp"
#include "util.hpp"

#include <gtest/gtest.h>

class BlockCacheTest : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::atomizer::block b0;
        b0.m_height = 44;
        b0.m_transactions.push_back(
            cbdc::test::simple_tx({'a'}, {{'b'}, {'c'}}, {{'d'}}));
        b0.m_transactions.push_back(
            cbdc::test::simple_tx({'E'}, {{'d'}, {'f'}}, {{'G'}}));
        b0.m_transactions.push_back(
            cbdc::test::simple_tx({'h'}, {{'i'}, {'j'}}, {{'k'}}));
        m_bc.push_block(std::move(b0));
    }

    cbdc::watchtower::block_cache m_bc{2};
};

TEST_F(BlockCacheTest, no_history) {
    ASSERT_FALSE(m_bc.check_spent({'Z'}).has_value());
    ASSERT_FALSE(m_bc.check_unspent({'Z'}).has_value());

    ASSERT_EQ(m_bc.best_block_height(), 44);
}

TEST_F(BlockCacheTest, spend_g) {
    ASSERT_FALSE(m_bc.check_spent({'G'}).has_value());
    ASSERT_EQ(m_bc.check_unspent({'G'}).value().first, 44);
    ASSERT_EQ(m_bc.check_unspent({'G'}).value().second, cbdc::hash_t{'E'});

    cbdc::atomizer::block b1;
    b1.m_height = 45;
    b1.m_transactions.push_back(
        cbdc::test::simple_tx({'L'}, {{'m'}, {'G'}}, {{'o'}}));
    m_bc.push_block(std::move(b1));

    ASSERT_FALSE(m_bc.check_unspent({'G'}).has_value());
    ASSERT_EQ(m_bc.check_spent({'G'}).value().first, 45);
    ASSERT_EQ(m_bc.check_spent({'G'}).value().second, cbdc::hash_t{'L'});
}

TEST_F(BlockCacheTest, add_k_plus_1) {
    ASSERT_FALSE(m_bc.check_spent({'G'}).has_value());
    ASSERT_EQ(m_bc.check_unspent({'G'}).value().first, 44);
    ASSERT_EQ(m_bc.check_unspent({'G'}).value().second, cbdc::hash_t{'E'});

    cbdc::atomizer::block b1;
    b1.m_height = 45;
    b1.m_transactions.push_back(
        cbdc::test::simple_tx({'l'}, {{'m'}, {'n'}}, {{'o'}}));
    b1.m_transactions.push_back(
        cbdc::test::simple_tx({'p'}, {{'q'}, {'r'}}, {{'s'}}));
    m_bc.push_block(std::move(b1));

    cbdc::atomizer::block b2;
    b2.m_height = 46;
    b2.m_transactions.push_back(
        cbdc::test::simple_tx({'t'}, {{'u'}, {'v'}}, {{'w'}}));
    m_bc.push_block(std::move(b2));

    ASSERT_FALSE(m_bc.check_spent({'G'}).has_value());
    ASSERT_FALSE(m_bc.check_unspent({'G'}).has_value());

    cbdc::atomizer::block b3;
    b3.m_height = 47;
    b3.m_transactions.push_back(
        cbdc::test::simple_tx({'X'}, {{'y'}, {'G'}}, {{'z'}}));
    m_bc.push_block(std::move(b3));

    ASSERT_FALSE(m_bc.check_unspent({'G'}).has_value());
    ASSERT_EQ(m_bc.check_spent({'G'}).value().first, 47);
    ASSERT_EQ(m_bc.check_spent({'G'}).value().second, cbdc::hash_t{'X'});

    ASSERT_EQ(m_bc.best_block_height(), 47);
}
