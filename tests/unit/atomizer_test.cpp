// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/atomizer/atomizer/atomizer.hpp"
#include "util.hpp"

#include <gtest/gtest.h>

class atomizer_test : public ::testing::Test {
  protected:
    void SetUp() override {
        static constexpr auto best_height = 0;
        static constexpr auto stxo_cache_depth = 2;
        m_atomizer
            = std::make_unique<cbdc::atomizer::atomizer>(best_height,
                                                         stxo_cache_depth);
    }
    void TearDown() override {}

    auto verify_serialization() {
        auto new_atomizer = std::make_unique<cbdc::atomizer::atomizer>(0, 0);
        auto ser = m_atomizer->serialize();
        auto ser_view = cbdc::buffer_serializer(ser);
        new_atomizer->deserialize(ser_view);
        ASSERT_EQ(*m_atomizer, *new_atomizer);
    }

    std::unique_ptr<cbdc::atomizer::atomizer> m_atomizer{};
};

TEST_F(atomizer_test, test_empty) {
    verify_serialization();
}

TEST_F(atomizer_test, test_with_transactions) {
    uint8_t val{0};
    static constexpr auto n_blocks = 10;
    static constexpr auto n_txs = 20;
    for(int h{0}; h < n_blocks; h++) {
        for(int i{0}; i < n_txs; i++) {
            cbdc::transaction::full_tx tx{};

            cbdc::transaction::input inp{};
            inp.m_prevout.m_tx_id = {val++};
            inp.m_prevout.m_index = val++;
            inp.m_prevout_data.m_witness_program_commitment = {val++};
            inp.m_prevout_data.m_value = val++;

            cbdc::transaction::output out;
            out.m_witness_program_commitment = {val++};
            out.m_value = val++;

            tx.m_inputs.push_back(inp);
            tx.m_outputs.push_back(out);

            const auto compact_tx = cbdc::transaction::compact_tx(tx);

            auto err = m_atomizer->insert(h, compact_tx, {0});
            ASSERT_FALSE(err.has_value());
        }

        auto errs = m_atomizer->make_block().second;
        ASSERT_TRUE(errs.empty());
    }

    verify_serialization();
}

TEST_F(atomizer_test, err_stxo_cache_depth_exceeded) {
    auto errs = m_atomizer->make_block().second;
    ASSERT_TRUE(errs.empty());

    auto tx0 = cbdc::test::simple_tx({'a'}, {{'b'}}, {{'c'}});
    auto err = m_atomizer->insert(1, tx0, {0});
    ASSERT_FALSE(err.has_value());
    errs = m_atomizer->make_block().second;
    ASSERT_TRUE(errs.empty());

    auto tx1 = cbdc::test::simple_tx({'d'}, {{'e'}}, {{'f'}});
    err = m_atomizer->insert(2, tx1, {0});
    ASSERT_FALSE(err.has_value());
    errs = m_atomizer->make_block().second;
    ASSERT_TRUE(errs.empty());

    auto tx_beyond_stxo_range = cbdc::test::simple_tx({'G'}, {{'h'}}, {{'i'}});
    err = m_atomizer->insert(0, tx_beyond_stxo_range, {0});
    auto want
        = cbdc::watchtower::tx_error{{'G'},
                                     cbdc::watchtower::tx_error_stxo_range{}};
    ASSERT_TRUE(err.has_value());
    ASSERT_EQ(err.value(), want);

    verify_serialization();
}

TEST_F(atomizer_test, err_inputs_spent) {
    auto errs = m_atomizer->make_block().second;
    ASSERT_TRUE(errs.empty());

    auto tx0 = cbdc::test::simple_tx({'a'}, {{'B'}}, {{'c'}});
    auto err = m_atomizer->insert(1, tx0, {0});
    ASSERT_FALSE(err.has_value());
    errs = m_atomizer->make_block().second;
    ASSERT_TRUE(errs.empty());

    auto tx1 = cbdc::test::simple_tx({'d'}, {{'E'}}, {{'f'}});
    err = m_atomizer->insert(2, tx1, {0});
    ASSERT_FALSE(err.has_value());
    errs = m_atomizer->make_block().second;
    ASSERT_TRUE(errs.empty());

    auto tx_inputs_spent
        = cbdc::test::simple_tx({'G'}, {{'E'}, {'h'}}, {{'i'}});
    err = m_atomizer->insert(2, tx_inputs_spent, {0, 1});

    auto want = cbdc::watchtower::tx_error{
        {'G'},
        cbdc::watchtower::tx_error_inputs_spent{{{'E'}}}};
    ASSERT_TRUE(err.has_value());
    ASSERT_EQ(err.value(), want);

    verify_serialization();
}

TEST_F(atomizer_test, err_incomplete) {
    auto [blk, errs] = m_atomizer->make_block();
    ASSERT_TRUE(errs.empty());

    auto tx_incomplete = cbdc::test::simple_tx({'A'}, {{'b'}, {'c'}}, {{'d'}});
    auto err = m_atomizer->insert(1, tx_incomplete, {1});
    ASSERT_FALSE(err.has_value());

    errs = m_atomizer->make_block().second;
    ASSERT_TRUE(errs.empty());
    errs = m_atomizer->make_block().second;
    ASSERT_TRUE(errs.empty());

    auto tx1 = cbdc::test::simple_tx({'e'}, {{'f'}, {'g'}}, {{'h'}});
    err = m_atomizer->insert(3, tx1, {0, 1});
    ASSERT_FALSE(err.has_value());

    errs = m_atomizer->make_block().second;
    ASSERT_EQ(errs.size(), 1UL);
    auto want
        = cbdc::watchtower::tx_error{{'A'},
                                     cbdc::watchtower::tx_error_incomplete{}};
    ASSERT_EQ(errs[0], want);

    verify_serialization();
}
