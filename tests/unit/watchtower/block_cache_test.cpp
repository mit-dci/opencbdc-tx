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
            cbdc::test::simple_tx({'a'},
                                  {{'b'}, {'c'}},
                                  {{{'d'}, {'e'}, {'f'}}}));
        b0.m_transactions.push_back(
            cbdc::test::simple_tx({'E'},
                                  {{'d'}, {'f'}},
                                  {{{'G'}, {'H'}, {'I'}}}));
        b0.m_transactions.push_back(
            cbdc::test::simple_tx({'h'},
                                  {{'i'}, {'j'}},
                                  {{{'k'}, {'l'}, {'m'}}}));
        m_bc.push_block(std::move(b0));
    }

    cbdc::watchtower::block_cache m_bc{2};
};

TEST_F(BlockCacheTest, no_history) {
    ASSERT_FALSE(m_bc.check_spent({'Z'}).has_value());
    ASSERT_FALSE(m_bc.check_unspent({'Z'}).has_value());

    ASSERT_EQ(m_bc.best_block_height(), 44UL);
}

TEST_F(BlockCacheTest, spend_g) {
    auto id = cbdc::transaction::calculate_uhs_id({{'G'}, {'H'}, {'I'}});
    ASSERT_FALSE(m_bc.check_spent(id).has_value());
    ASSERT_EQ(m_bc.check_unspent(id).value().first, 44UL);
    ASSERT_EQ(m_bc.check_unspent(id).value().second, cbdc::hash_t{'E'});

    cbdc::atomizer::block b1;
    b1.m_height = 45;
    b1.m_transactions.push_back(
        cbdc::test::simple_tx({'L'}, {{'m'}, id}, {{{'o'}, {'p'}, {'q'}}}));
    m_bc.push_block(std::move(b1));

    ASSERT_FALSE(m_bc.check_unspent(id).has_value());
    ASSERT_EQ(m_bc.check_spent(id).value().first, 45UL);
    ASSERT_EQ(m_bc.check_spent(id).value().second, cbdc::hash_t{'L'});
}

TEST_F(BlockCacheTest, add_k_plus_1) {
    auto id = cbdc::transaction::calculate_uhs_id({{'G'}, {'H'}, {'I'}});
    ASSERT_FALSE(m_bc.check_spent(id).has_value());
    ASSERT_EQ(m_bc.check_unspent(id).value().first, 44UL);
    ASSERT_EQ(m_bc.check_unspent(id).value().second, cbdc::hash_t{'E'});

    cbdc::atomizer::block b1;
    b1.m_height = 45;
    b1.m_transactions.push_back(
        cbdc::test::simple_tx({'l'}, {{'m'}, {'n'}}, {{{'o'}, {'p'}, {'q'}}}));
    b1.m_transactions.push_back(
        cbdc::test::simple_tx({'p'}, {{'q'}, {'r'}}, {{{'s'}, {'t'}, {'u'}}}));
    m_bc.push_block(std::move(b1));

    cbdc::atomizer::block b2;
    b2.m_height = 46;
    b2.m_transactions.push_back(
        cbdc::test::simple_tx({'t'}, {{'u'}, {'v'}}, {{{'w'}, {'x'}, {'y'}}}));
    m_bc.push_block(std::move(b2));

    ASSERT_FALSE(m_bc.check_spent(id).has_value());
    ASSERT_FALSE(m_bc.check_unspent(id).has_value());

    cbdc::atomizer::block b3;
    b3.m_height = 47;
    b3.m_transactions.push_back(
        cbdc::test::simple_tx({'X'}, {{'y'}, id}, {{{'z'}, {'a'}, {'b'}}}));
    m_bc.push_block(std::move(b3));

    ASSERT_FALSE(m_bc.check_unspent(id).has_value());
    ASSERT_EQ(m_bc.check_spent(id).value().first, 47UL);
    ASSERT_EQ(m_bc.check_spent(id).value().second, cbdc::hash_t{'X'});

    ASSERT_EQ(m_bc.best_block_height(), 47UL);
}
