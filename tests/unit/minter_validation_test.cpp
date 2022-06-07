// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"

#include <gtest/gtest.h>
#include <variant>

class MinterValidationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        const auto minter_pub = m_minter.generate_minter_key();
        m_opts.m_minter_pubkeys.insert(minter_pub);
    }

    cbdc::transaction::wallet m_minter{};
    cbdc::transaction::wallet m_not_minter{};
    cbdc::config::options m_opts{};
};

TEST_F(MinterValidationTest, valid_mint) {
    cbdc::transaction::full_tx tx = m_minter.mint_new_coins(5, 10);
    auto err
        = cbdc::transaction::validation::check_tx(tx, m_opts.m_minter_pubkeys);
    ASSERT_FALSE(err.has_value());
}

TEST_F(MinterValidationTest, invalid_mint) {
    cbdc::transaction::full_tx tx = m_not_minter.mint_new_coins(1, 1000);
    auto err = cbdc::transaction::validation::check_mint_p2pk_witness(
        tx,
        0,
        m_opts.m_minter_pubkeys);
    ASSERT_TRUE(err.has_value());
    ASSERT_EQ(
        err.value(),
        cbdc::transaction::validation::witness_error_code::invalid_minter_key);
}

TEST_F(MinterValidationTest, no_outputs) {
    cbdc::transaction::full_tx tx = m_minter.mint_new_coins(5, 10);
    tx.m_outputs.clear();

    auto err
        = cbdc::transaction::validation::check_tx(tx, m_opts.m_minter_pubkeys);
    ASSERT_TRUE(err.has_value());
}

TEST_F(MinterValidationTest, no_output_value) {
    cbdc::transaction::full_tx tx = m_minter.mint_new_coins(5, 10);
    tx.m_outputs[0].m_value = 0;

    auto err
        = cbdc::transaction::validation::check_tx(tx, m_opts.m_minter_pubkeys);
    ASSERT_TRUE(err.has_value());
}

TEST_F(MinterValidationTest, missing_witness) {
    cbdc::transaction::full_tx tx = m_minter.mint_new_coins(5, 10);
    tx.m_witness.clear();

    auto err
        = cbdc::transaction::validation::check_tx(tx, m_opts.m_minter_pubkeys);
    ASSERT_TRUE(err.has_value());
}

TEST_F(MinterValidationTest, bad_witness_committment) {
    cbdc::transaction::full_tx tx = m_minter.mint_new_coins(5, 10);
    tx.m_outputs[0].m_witness_program_commitment = cbdc::hash_t{0};

    auto err
        = cbdc::transaction::validation::check_tx(tx, m_opts.m_minter_pubkeys);
    ASSERT_TRUE(err.has_value());
}
