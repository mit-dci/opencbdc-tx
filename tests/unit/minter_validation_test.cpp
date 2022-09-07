// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
//               2022 MITRE Corporation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"

#include <gtest/gtest.h>
#include <variant>

class MinterValidationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        const auto minter_private_key = "000000000000000200000000000000000"
                                        "0000000000000000000000000000000";
        const auto minter_public_key = "3adb9db3beb997eec2623ea5002279ea9e"
                                       "337b5c705f3db453dbc1cc1fc9b0a8";

        m_opts.m_minter_private_keys[0]
            = cbdc::hash_from_hex(minter_private_key);
        m_opts.m_minter_public_keys.insert(
            cbdc::hash_from_hex(minter_public_key));
    }

    cbdc::transaction::wallet m_minter{};
    cbdc::config::options m_opts{};
};

TEST_F(MinterValidationTest, valid_mint) {
    auto otx = m_minter.mint_new_coins(5, 10, m_opts, 0);
    ASSERT_TRUE(otx.has_value());
    auto tx = otx.value();

    ASSERT_EQ(tx.m_outputs.size(), 5);
    ASSERT_EQ(tx.m_inputs.size(), 0);
    m_minter.confirm_transaction(tx);
    ASSERT_EQ(m_minter.balance(), 50);

    auto err
        = cbdc::transaction::validation::check_tx(tx,
                                                  m_opts.m_minter_public_keys);
    ASSERT_FALSE(err.has_value());
}

TEST_F(MinterValidationTest, invalid_minter_key) {
    // Bad minter key index
    auto tx = m_minter.mint_new_coins(5, 10, m_opts, 5);
    ASSERT_FALSE(tx.has_value());
}

TEST_F(MinterValidationTest, no_outputs) {
    auto otx = m_minter.mint_new_coins(5, 10, m_opts, 0);
    ASSERT_TRUE(otx.has_value());
    auto tx = otx.value();

    tx.m_outputs.clear();

    auto err
        = cbdc::transaction::validation::check_tx(tx,
                                                  m_opts.m_minter_public_keys);
    ASSERT_TRUE(err.has_value());
}

TEST_F(MinterValidationTest, no_output_value) {
    auto otx = m_minter.mint_new_coins(5, 10, m_opts, 0);
    ASSERT_TRUE(otx.has_value());
    auto tx = otx.value();

    tx.m_outputs[0].m_value = 0;

    auto err
        = cbdc::transaction::validation::check_tx(tx,
                                                  m_opts.m_minter_public_keys);
    ASSERT_TRUE(err.has_value());
}

TEST_F(MinterValidationTest, missing_witness) {
    auto otx = m_minter.mint_new_coins(5, 10, m_opts, 0);
    ASSERT_TRUE(otx.has_value());
    auto tx = otx.value();

    tx.m_witness.clear();

    auto err
        = cbdc::transaction::validation::check_tx(tx,
                                                  m_opts.m_minter_public_keys);
    ASSERT_TRUE(err.has_value());
}

TEST_F(MinterValidationTest, bad_witness_committment) {
    auto otx = m_minter.mint_new_coins(5, 10, m_opts, 0);
    ASSERT_TRUE(otx.has_value());
    auto tx = otx.value();

    tx.m_outputs[0].m_witness_program_commitment = cbdc::hash_t{0};

    auto err
        = cbdc::transaction::validation::check_tx(tx,
                                                  m_opts.m_minter_public_keys);
    ASSERT_TRUE(err.has_value());
}
