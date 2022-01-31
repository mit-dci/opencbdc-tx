// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"

#include <gtest/gtest.h>
#include <variant>

// TODO: Redo, removing dependence on Wallet.
class WalletTxValidationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::transaction::wallet wallet1;
        cbdc::transaction::wallet wallet2;

        auto mint_tx1 = wallet1.mint_new_coins(3, 100);
        wallet1.confirm_transaction(mint_tx1);
        auto mint_tx2 = wallet2.mint_new_coins(1, 100);
        wallet2.confirm_transaction(mint_tx2);

        m_valid_tx = wallet1.send_to(20, wallet2.generate_key(), true).value();
        m_valid_tx_multi_inp
            = wallet1.send_to(200, wallet2.generate_key(), true).value();
    }

    cbdc::transaction::full_tx m_valid_tx{};
    cbdc::transaction::full_tx m_valid_tx_multi_inp{};
};

TEST_F(WalletTxValidationTest, valid) {
    auto err = cbdc::transaction::validation::check_tx(m_valid_tx);
    ASSERT_FALSE(err.has_value());
}

TEST_F(WalletTxValidationTest, no_inputs) {
    m_valid_tx.m_inputs.clear();

    auto err = cbdc::transaction::validation::check_tx(m_valid_tx);
    ASSERT_TRUE(err.has_value());
    ASSERT_TRUE(
        std::holds_alternative<cbdc::transaction::validation::tx_error_code>(
            err.value()));

    auto tx_err
        = std::get<cbdc::transaction::validation::tx_error_code>(err.value());

    ASSERT_EQ(tx_err, cbdc::transaction::validation::tx_error_code::no_inputs);
}

TEST_F(WalletTxValidationTest, no_outputs) {
    m_valid_tx.m_outputs.clear();

    auto err = cbdc::transaction::validation::check_tx(m_valid_tx);
    ASSERT_TRUE(err.has_value());
    ASSERT_TRUE(
        std::holds_alternative<cbdc::transaction::validation::tx_error_code>(
            err.value()));

    auto tx_err
        = std::get<cbdc::transaction::validation::tx_error_code>(err.value());

    ASSERT_EQ(tx_err,
              cbdc::transaction::validation::tx_error_code::no_outputs);
}

TEST_F(WalletTxValidationTest, missing_witness) {
    m_valid_tx.m_witness.clear();

    auto err = cbdc::transaction::validation::check_tx(m_valid_tx);
    ASSERT_TRUE(err.has_value());
    ASSERT_TRUE(
        std::holds_alternative<cbdc::transaction::validation::tx_error_code>(
            err.value()));

    auto tx_err
        = std::get<cbdc::transaction::validation::tx_error_code>(err.value());

    ASSERT_EQ(tx_err,
              cbdc::transaction::validation::tx_error_code::missing_witness);
}

TEST_F(WalletTxValidationTest, zero_output) {
    m_valid_tx.m_outputs[0].m_value = 0;

    auto err = cbdc::transaction::validation::check_tx(m_valid_tx);

    ASSERT_TRUE(err.has_value());
    ASSERT_TRUE(
        std::holds_alternative<cbdc::transaction::validation::output_error>(
            err.value()));

    auto output_err
        = std::get<cbdc::transaction::validation::output_error>(err.value());

    ASSERT_EQ(output_err.m_idx, uint64_t{0});
    ASSERT_EQ(output_err.m_code,
              cbdc::transaction::validation::output_error_code::zero_value);
}

TEST_F(WalletTxValidationTest, duplicate_input) {
    m_valid_tx.m_inputs.emplace_back(m_valid_tx.m_inputs[0]);
    m_valid_tx.m_witness.emplace_back(m_valid_tx.m_witness[0]);
    m_valid_tx.m_outputs[0].m_value *= 2;

    auto err = cbdc::transaction::validation::check_tx(m_valid_tx);

    ASSERT_TRUE(err.has_value());
    ASSERT_TRUE(
        std::holds_alternative<cbdc::transaction::validation::input_error>(
            err.value()));

    auto input_err
        = std::get<cbdc::transaction::validation::input_error>(err.value());

    ASSERT_EQ(input_err.m_idx, uint64_t{1});
    ASSERT_EQ(input_err.m_code,
              cbdc::transaction::validation::input_error_code::duplicate);
}

TEST_F(WalletTxValidationTest, invalid_input_prevout) {
    m_valid_tx.m_inputs[0].m_prevout_data.m_value = 0;
    auto err = cbdc::transaction::validation::check_tx(m_valid_tx);

    ASSERT_TRUE(err.has_value());
    ASSERT_TRUE(
        std::holds_alternative<cbdc::transaction::validation::input_error>(
            err.value()));

    auto input_err
        = std::get<cbdc::transaction::validation::input_error>(err.value());

    ASSERT_EQ(input_err.m_idx, uint64_t{0});
    ASSERT_EQ(input_err.m_code,
              cbdc::transaction::validation::input_error_code::data_error);
}

TEST_F(WalletTxValidationTest, asymmetric_inout_set) {
    m_valid_tx.m_outputs[0].m_value--;
    auto err = cbdc::transaction::validation::check_tx(m_valid_tx);

    ASSERT_TRUE(err.has_value());
    ASSERT_TRUE(
        std::holds_alternative<cbdc::transaction::validation::tx_error_code>(
            err.value()));

    auto tx_err
        = std::get<cbdc::transaction::validation::tx_error_code>(err.value());

    ASSERT_EQ(tx_err,
              cbdc::transaction::validation::tx_error_code::asymmetric_values);
}

TEST_F(WalletTxValidationTest, witness_missing_witness_program_type) {
    m_valid_tx.m_witness[0].clear();
    auto err = cbdc::transaction::validation::check_witness(m_valid_tx, 0);
    ASSERT_TRUE(err.has_value());
    ASSERT_EQ(err.value(),
              cbdc::transaction::validation::witness_error_code::
                  missing_witness_program_type);
}

TEST_F(WalletTxValidationTest, witness_unknown_witness_program_type) {
    m_valid_tx.m_witness[0][0] = std::byte(0xFF);
    auto err = cbdc::transaction::validation::check_witness(m_valid_tx, 0);
    ASSERT_TRUE(err.has_value());
    ASSERT_EQ(err.value(),
              cbdc::transaction::validation::witness_error_code::
                  unknown_witness_program_type);
}

TEST_F(WalletTxValidationTest, witness_invalid_p2pk_len) {
    m_valid_tx.m_witness[0].resize(
        cbdc::transaction::validation::p2pk_witness_len - 1);
    auto err = cbdc::transaction::validation::check_witness(m_valid_tx, 0);
    ASSERT_TRUE(err.has_value());
    ASSERT_EQ(err.value(),
              cbdc::transaction::validation::witness_error_code::malformed);
}

TEST_F(WalletTxValidationTest, witness_p2pk_program_mismatch) {
    m_valid_tx.m_inputs[0].m_prevout_data.m_witness_program_commitment
        = cbdc::hash_t{0};
    auto err = cbdc::transaction::validation::check_witness(m_valid_tx, 0);
    ASSERT_TRUE(err.has_value());
    ASSERT_EQ(
        err.value(),
        cbdc::transaction::validation::witness_error_code::program_mismatch);
}

TEST_F(WalletTxValidationTest, witness_p2pk_invalid_pubkey) {
    // pubkey all zeroes is invalid
    auto invalid_pubkey = cbdc::witness_t(32, std::byte(0));
    std::copy(
        std::begin(invalid_pubkey),
        std::end(invalid_pubkey),
        std::begin(m_valid_tx.m_witness[0])
            + sizeof(cbdc::transaction::validation::witness_program_type));
    // Recalculate the witness program commitment - otherwise it'll fail with
    // ::ProgramMismatch
    m_valid_tx.m_inputs[0].m_prevout_data.m_witness_program_commitment
        = cbdc::hash_data(
            m_valid_tx.m_witness[0].data(),
            cbdc::transaction::validation::p2pk_witness_prog_len);
    auto err = cbdc::transaction::validation::check_witness(m_valid_tx, 0);
    ASSERT_TRUE(err.has_value());
    ASSERT_EQ(
        err.value(),
        cbdc::transaction::validation::witness_error_code::invalid_public_key);
}

TEST_F(WalletTxValidationTest, witness_p2pk_invalid_signature) {
    // Alter the first byte of the signature.
    m_valid_tx
        .m_witness[0][cbdc::transaction::validation::p2pk_witness_prog_len]
        = std::byte(
            uint8_t(
                m_valid_tx.m_witness
                    [0][cbdc::transaction::validation::p2pk_witness_prog_len])
            + 1);
    // Recalculate the witness program commitment - otherwise it'll fail with
    // ::ProgramMismatch
    m_valid_tx.m_inputs[0].m_prevout_data.m_witness_program_commitment
        = cbdc::hash_data(
            m_valid_tx.m_witness[0].data(),
            cbdc::transaction::validation::p2pk_witness_prog_len);
    auto err = cbdc::transaction::validation::check_witness(m_valid_tx, 0);
    ASSERT_TRUE(err.has_value());
    ASSERT_EQ(
        err.value(),
        cbdc::transaction::validation::witness_error_code::invalid_signature);
}

TEST_F(WalletTxValidationTest,
       check_transaction_with_unknown_witness_program_type) {
    m_valid_tx.m_witness[0][0] = std::byte(0xFF);
    auto err = cbdc::transaction::validation::check_tx(m_valid_tx);
    ASSERT_TRUE(err.has_value());
    ASSERT_TRUE(
        std::holds_alternative<cbdc::transaction::validation::witness_error>(
            err.value()));

    auto wit_err
        = std::get<cbdc::transaction::validation::witness_error>(err.value());

    ASSERT_EQ(wit_err.m_idx, uint64_t{0});
    ASSERT_EQ(wit_err.m_code,
              cbdc::transaction::validation::witness_error_code::
                  unknown_witness_program_type);
}

TEST_F(WalletTxValidationTest, check_to_string) {
    auto in_err = cbdc::transaction::validation::tx_error{
        cbdc::transaction::validation::input_error{
            cbdc::transaction::validation::input_error_code::duplicate,
            std::nullopt,
            12}};
    auto out_err = cbdc::transaction::validation::tx_error{
        cbdc::transaction::validation::output_error{
            cbdc::transaction::validation::output_error_code::zero_value,
            82}};
    auto wit_err = cbdc::transaction::validation::tx_error{
        cbdc::transaction::validation::witness_error{
            cbdc::transaction::validation::witness_error_code::
                program_mismatch,
            17}};
    auto tx_err = cbdc::transaction::validation::tx_error{
        cbdc::transaction::validation::tx_error_code::no_inputs};

    ASSERT_EQ(cbdc::transaction::validation::to_string(wit_err),
              "Witness error (idx: 17): Witness commitment does not match "
              "witness program");
    ASSERT_EQ(cbdc::transaction::validation::to_string(out_err),
              "Output error (idx: 82): Output has zero value");
    ASSERT_EQ(cbdc::transaction::validation::to_string(in_err),
              "Input error (idx: 12): Duplicate outpoint");
    ASSERT_EQ(cbdc::transaction::validation::to_string(tx_err),
              "TX error: No inputs");
}

TEST_F(WalletTxValidationTest, summation_overflow) {
    auto inp_tx = m_valid_tx_multi_inp;
    inp_tx.m_inputs[0].m_prevout_data.m_value
        = std::numeric_limits<uint64_t>::max();
    auto res = cbdc::transaction::validation::check_in_out_set(inp_tx);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(
        res.value(),
        cbdc::transaction::validation::tx_error(
            cbdc::transaction::validation::tx_error_code::value_overflow));

    auto out_tx = m_valid_tx_multi_inp;
    out_tx.m_outputs[0].m_value = std::numeric_limits<uint64_t>::max();
    res = cbdc::transaction::validation::check_in_out_set(inp_tx);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(
        res.value(),
        cbdc::transaction::validation::tx_error(
            cbdc::transaction::validation::tx_error_code::value_overflow));
}
