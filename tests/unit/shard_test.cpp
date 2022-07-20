// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/atomizer/shard/shard.hpp"
#include "uhs/transaction/wallet.hpp"
#include "util.hpp"

#include <filesystem>
#include <gtest/gtest.h>

static constexpr auto g_shard_test_dir = "test_shard_db";

TEST(shard_sync_test, digest_tx_sync_err) {
    cbdc::shard::shard m_shard{{3, 8}};

    cbdc::transaction::compact_tx ctx{};
    ctx.m_id = {'a'};

    auto res = m_shard.digest_transaction(ctx);
    EXPECT_TRUE(std::holds_alternative<cbdc::watchtower::tx_error>(res));
    if(auto* got = std::get_if<cbdc::watchtower::tx_error>(&res)) {
        cbdc::watchtower::tx_error want{{'a'},
                                        cbdc::watchtower::tx_error_sync{}};
        EXPECT_EQ(*got, want);
    }

    std::filesystem::remove_all(g_shard_test_dir);
}

class shard_test : public ::testing::Test {
  protected:
    void SetUp() override {
        m_shard.open_db(g_shard_test_dir);

        cbdc::atomizer::block b1;
        b1.m_height = 1;
        b1.m_transactions.push_back(
            cbdc::test::simple_tx({'a'}, {}, {{3}, {4}}));
        b1.m_transactions.push_back(
            cbdc::test::simple_tx({'b'}, {}, {{5}, {6}}));
        m_shard.digest_block(b1);
    }

    void TearDown() override {
        std::filesystem::remove_all(g_shard_test_dir);
    }

    cbdc::shard::shard m_shard{{3, 8}};
};

TEST_F(shard_test, digest_block_non_contiguous) {
    cbdc::atomizer::block b44;
    b44.m_height = 44;
    ASSERT_FALSE(m_shard.digest_block(b44));
}

TEST_F(shard_test, digest_tx_valid) {
    cbdc::transaction::compact_tx ctx{};
    ctx.m_id = {'a'};
    ctx.m_inputs = {{0}, {3}, {6}, {100}};
    ctx.m_outputs
        = {{{'b'}, {'c'}, {'d'}, {'e'}}, {{'h'}, {'i'}, {'j'}, {'k'}}};

    auto res = m_shard.digest_transaction(ctx);
    ASSERT_TRUE(
        std::holds_alternative<cbdc::atomizer::tx_notify_request>(res));
    auto got = std::get<cbdc::atomizer::tx_notify_request>(res);

    cbdc::atomizer::tx_notify_request want{};
    want.m_tx = ctx;
    want.m_attestations = {1, 2};
    want.m_block_height = 1;

    ASSERT_EQ(got, want);
}

TEST_F(shard_test, digest_tx_empty_inputs) {
    cbdc::transaction::compact_tx ctx{};
    ctx.m_id = {'a'};
    ctx.m_inputs = {};
    ctx.m_outputs
        = {{{'b'}, {'c'}, {'d'}, {'e'}}, {{'h'}, {'i'}, {'j'}, {'k'}}};

    auto res = m_shard.digest_transaction(ctx);
    ASSERT_TRUE(std::holds_alternative<cbdc::watchtower::tx_error>(res));
    auto got = std::get<cbdc::watchtower::tx_error>(res);

    cbdc::watchtower::tx_error want{{'a'},
                                    cbdc::watchtower::tx_error_inputs_dne{{}}};

    ASSERT_EQ(got, want);
}

TEST_F(shard_test, digest_tx_inputs_dne) {
    cbdc::transaction::compact_tx ctx{};
    ctx.m_id = {'a'};
    ctx.m_inputs = {{0}, {7}, {8}, {100}};
    ctx.m_outputs
        = {{{'b'}, {'c'}, {'d'}, {'e'}}, {{'h'}, {'i'}, {'j'}, {'k'}}};

    auto res = m_shard.digest_transaction(ctx);
    ASSERT_TRUE(std::holds_alternative<cbdc::watchtower::tx_error>(res));
    auto got = std::get<cbdc::watchtower::tx_error>(res);

    cbdc::watchtower::tx_error want{
        {'a'},
        cbdc::watchtower::tx_error_inputs_dne{{{7}, {8}}}};

    ASSERT_EQ(got, want);
}

TEST_F(shard_test, digest_block_valid) {
    cbdc::atomizer::block b2;
    b2.m_height = 2;
    b2.m_transactions.push_back(
        cbdc::test::simple_tx({'c'}, {{1}, {3}, {4}, {11}}, {{7}}));
    b2.m_transactions.push_back(
        cbdc::test::simple_tx({'d'}, {{2}, {5}, {6}, {22}}, {{8}}));
    m_shard.digest_block(b2);

    cbdc::transaction::compact_tx valid_ctx{};
    valid_ctx.m_id = {'a'};
    valid_ctx.m_inputs = {{0}, {7}, {100}, {8}};
    valid_ctx.m_outputs
        = {{{'b'}, {'c'}, {'d'}, {'e'}}, {{'h'}, {'i'}, {'j'}, {'k'}}};

    auto valid_res = m_shard.digest_transaction(valid_ctx);
    ASSERT_TRUE(
        std::holds_alternative<cbdc::atomizer::tx_notify_request>(valid_res));
    auto valid_got = std::get<cbdc::atomizer::tx_notify_request>(valid_res);

    cbdc::atomizer::tx_notify_request valid_want{};
    valid_want.m_tx = valid_ctx;
    valid_want.m_attestations = {1, 3};
    valid_want.m_block_height = 2;

    ASSERT_EQ(valid_got, valid_want);

    cbdc::transaction::compact_tx invalid_ctx{};
    invalid_ctx.m_id = {'a'};
    invalid_ctx.m_inputs = {{0}, {3}, {4}, {5}, {6}, {100}};
    invalid_ctx.m_outputs
        = {{{'b'}, {'c'}, {'d'}, {'e'}}, {{'h'}, {'i'}, {'j'}, {'k'}}};
    auto invalid_res = m_shard.digest_transaction(invalid_ctx);
    ASSERT_TRUE(
        std::holds_alternative<cbdc::watchtower::tx_error>(invalid_res));
    auto invalid_got = std::get<cbdc::watchtower::tx_error>(invalid_res);

    cbdc::watchtower::tx_error invalid_want{
        {'a'},
        cbdc::watchtower::tx_error_inputs_dne{{{3}, {4}, {5}, {6}}}};

    ASSERT_EQ(invalid_got, invalid_want);
}
