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
        m_mint_tx1 = wallet1.mint_new_coins(3, 100);
        wallet1.confirm_transaction(m_mint_tx1);
        auto mint_tx2 = wallet2.mint_new_coins(1, 100);
        wallet2.confirm_transaction(mint_tx2);

        m_valid_tx = wallet1.send_to(20, wallet2.generate_key(), true).value();
        m_valid_tx_multi_inp
            = wallet1.send_to(200, wallet2.generate_key(), true).value();
    }

    cbdc::transaction::wallet wallet1;
    cbdc::transaction::wallet wallet2;

    cbdc::transaction::full_tx m_mint_tx1{};
    cbdc::transaction::full_tx m_valid_tx{};
    cbdc::transaction::full_tx m_valid_tx_multi_inp{};

    using secp256k1_context_destroy_type = void (*)(secp256k1_context*);

    std::unique_ptr<secp256k1_context,
                    secp256k1_context_destroy_type>
        m_secp{secp256k1_context_create(SECP256K1_CONTEXT_NONE),
               &secp256k1_context_destroy};

    cbdc::privkey_t m_priv0{cbdc::hash_from_hex(
        "0000000000000001000000000000000000000000000000000000000000000000")};
    cbdc::pubkey_t m_pub0{cbdc::pubkey_from_privkey(m_priv0, m_secp.get())};
    cbdc::privkey_t m_priv1{cbdc::hash_from_hex(
        "1000000000000001000000000000000000000000000000000000000000000000")};
    cbdc::pubkey_t m_pub1{cbdc::pubkey_from_privkey(m_priv1, m_secp.get())};
    std::unordered_set<cbdc::pubkey_t, cbdc::hashing::null> m_pubkeys{m_pub0,
                                                                      m_pub1};
};

TEST_F(WalletTxValidationTest, ins_and_outs) {
    for(size_t i = 1; i <= 3; ++i) {
        cbdc::transaction::wallet a{};
        cbdc::transaction::wallet b{};

        auto mint_tx = a.mint_new_coins(4, 5 * i);
        a.confirm_transaction(mint_tx);

        auto one_to_one = a.send_to(5 * i, b.generate_key(), true);
        EXPECT_TRUE(one_to_one.has_value());
        EXPECT_EQ(one_to_one.value().m_inputs.size(), 1UL);
        EXPECT_EQ(one_to_one.value().m_outputs.size(), 1UL);
        auto err = cbdc::transaction::validation::check_tx(one_to_one.value());
        EXPECT_FALSE(err.has_value());

        auto one_to_two = a.send_to(5 * i - 1, b.generate_key(), true);
        EXPECT_TRUE(one_to_two.has_value());
        EXPECT_EQ(one_to_two.value().m_inputs.size(), 1UL);
        EXPECT_EQ(one_to_two.value().m_outputs.size(), 2UL);
        err = cbdc::transaction::validation::check_tx(one_to_two.value());
        EXPECT_FALSE(err.has_value());

        auto two_to_two = a.send_to(5 * i + 1, b.generate_key(), true);
        EXPECT_TRUE(two_to_two.has_value());
        EXPECT_EQ(two_to_two.value().m_inputs.size(), 2UL);
        EXPECT_EQ(two_to_two.value().m_outputs.size(), 2UL);
        err = cbdc::transaction::validation::check_tx(two_to_two.value());
        EXPECT_FALSE(err.has_value());

        // todo: should we disallow transactions which send only minted
        // outputs, and have only one output?
        //
        // reasoning: such a tx guarantees an output blinding factor of 0
    }
}

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
    auto inval = wallet1.send_to(0, wallet2.generate_key(), true);
    ASSERT_FALSE(inval.has_value());
}

TEST_F(WalletTxValidationTest, duplicate_input) {
    m_valid_tx.m_inputs.emplace_back(m_valid_tx.m_inputs[0]);
    m_valid_tx.m_witness.emplace_back(m_valid_tx.m_witness[0]);
    m_valid_tx.m_out_spend_data.value()[0].m_value *= 2;

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

TEST_F(WalletTxValidationTest, asymmetric_inout_set) {
    cbdc::transaction::compact_tx ctx(m_mint_tx1);
    auto err = cbdc::transaction::validation::check_proof(ctx, {});
    ASSERT_TRUE(err.has_value());
    ASSERT_EQ(err.value().m_code,
              cbdc::transaction::validation::proof_error_code::wrong_sum);
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

TEST_F(WalletTxValidationTest, sign_verify_compact) {
    auto ctx = cbdc::transaction::compact_tx(m_valid_tx);
    auto att0 = ctx.sign(m_secp.get(), m_priv0);
    auto att1 = ctx.sign(m_secp.get(), m_priv1);
    ASSERT_TRUE(ctx.verify(m_secp.get(), att0));
    ASSERT_TRUE(ctx.verify(m_secp.get(), att1));

    ASSERT_FALSE(
        cbdc::transaction::validation::check_attestations(ctx, m_pubkeys, 2));

    ctx.m_attestations.insert(att0);
    ASSERT_FALSE(
        cbdc::transaction::validation::check_attestations(ctx, m_pubkeys, 2));

    ctx.m_attestations.insert(att1);
    ASSERT_TRUE(
        cbdc::transaction::validation::check_attestations(ctx, m_pubkeys, 2));

    m_pubkeys.clear();
    ASSERT_FALSE(
        cbdc::transaction::validation::check_attestations(ctx, m_pubkeys, 2));
}
