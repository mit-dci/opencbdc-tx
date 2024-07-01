// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"

#include <filesystem>
#include <gtest/gtest.h>

class WalletTest : public ::testing::Test {
  protected:
    void SetUp() override {
        auto mint_tx = m_wallet.mint_new_coins(1, 100);
        m_wallet.confirm_transaction(mint_tx);
    }

    void TearDown() override {
        std::filesystem::remove(m_wallet_file);
    }

    cbdc::transaction::wallet m_wallet{};
    static constexpr auto m_wallet_file = "test_wallet.dat";
};

TEST_F(WalletTest, update_balance_basic) {
    m_wallet.confirm_transaction(m_wallet.mint_new_coins(1, 14));
    m_wallet.confirm_transaction(m_wallet.mint_new_coins(1, 22));
    ASSERT_EQ(m_wallet.balance(), uint32_t{136});
}

TEST_F(WalletTest, update_balance_double_credit) {
    m_wallet.confirm_transaction(m_wallet.mint_new_coins(1, 14));
    ASSERT_EQ(m_wallet.balance(), uint32_t{114});
}

TEST_F(WalletTest, export_send_input_basic) {
    cbdc::pubkey_t target_addr = {'a', 'b', 'c', 'd'};
    auto send_tx = m_wallet.send_to(25, target_addr, false).value();
    auto receiver_inputs
        = cbdc::transaction::wallet::export_send_inputs(send_tx, target_addr);
    ASSERT_EQ(receiver_inputs.size(), 1UL);

    ASSERT_EQ(receiver_inputs[0].m_prevout.m_tx_id,
              cbdc::transaction::tx_id(send_tx));
    ASSERT_TRUE(receiver_inputs[0].m_spend_data.has_value());
    ASSERT_EQ(receiver_inputs[0].m_spend_data.value().m_value, 25U);
}

TEST_F(WalletTest, fan_out) {
    cbdc::pubkey_t target_addr = {'a', 'b', 'c', 'd'};
    auto send_tx = m_wallet.fan(20, 5, target_addr, false).value();
    ASSERT_EQ(send_tx.m_outputs.size(), 20UL);
    auto witcom = cbdc::transaction::validation::get_p2pk_witness_commitment(
        target_addr);
    for(size_t i = 0; i < send_tx.m_outputs.size(); ++i) {
        auto out = send_tx.m_outputs[i];
        auto spend = send_tx.m_out_spend_data.value()[i];
        ASSERT_EQ(spend.m_value, 5UL);
        ASSERT_EQ(out.m_witness_program_commitment, witcom);
    }
    auto receiver_inputs
        = cbdc::transaction::wallet::export_send_inputs(send_tx, target_addr);
    ASSERT_EQ(receiver_inputs.size(), 20UL);
}

TEST_F(WalletTest, fan_out_change) {
    cbdc::pubkey_t target_addr = {'a', 'b', 'c', 'd'};
    auto send_tx = m_wallet.fan(19, 5, target_addr, false).value();
    ASSERT_EQ(send_tx.m_outputs.size(), 20UL);
    auto witcom = cbdc::transaction::validation::get_p2pk_witness_commitment(
        target_addr);
    for(size_t i = 1; i < send_tx.m_outputs.size(); i++) {
        ASSERT_EQ(send_tx.m_out_spend_data.value()[i].m_value, 5UL);
        ASSERT_EQ(send_tx.m_outputs[i].m_witness_program_commitment, witcom);
    }
    ASSERT_EQ(send_tx.m_out_spend_data.value()[0].m_value, 5UL);
    ASSERT_NE(send_tx.m_outputs[0].m_witness_program_commitment, witcom);
    auto receiver_inputs
        = cbdc::transaction::wallet::export_send_inputs(send_tx, target_addr);
    ASSERT_EQ(receiver_inputs.size(), 19UL);
}

class WalletTxTest : public ::testing::Test {
  protected:
    void SetUp() override {
        auto mint_tx = m_sender.mint_new_coins(1, 100);
        m_sender.confirm_transaction(mint_tx);
    }

    cbdc::transaction::wallet m_sender{};
    cbdc::transaction::wallet m_receiver{};
};

TEST_F(WalletTxTest, basic) {
    auto target_key = m_receiver.generate_key();

    auto send_tx = m_sender.send_to(20, target_key, true).value();
    m_sender.confirm_transaction(send_tx);
    m_receiver.confirm_transaction(send_tx);

    ASSERT_EQ(m_sender.balance(), uint32_t{80});
    ASSERT_EQ(m_receiver.balance(), uint32_t{20});
}

class WalletMultiTxTest : public ::testing::Test {
  protected:
    void SetUp() override {
        auto mint_tx = m_sender.mint_new_coins(100, 100);
        m_sender.confirm_transaction(mint_tx);
    }

    cbdc::transaction::wallet m_sender{};
};

TEST_F(WalletMultiTxTest, inp_out_count) {
    auto target_key = m_sender.generate_key();
    auto balance = m_sender.balance();
    auto count = m_sender.count();
    for(size_t inps{1}; inps < 5; inps++) {
        for(size_t outs{1}; outs < 5; outs++) {
            auto send_tx = m_sender.send_to(inps, outs, target_key, true);
            ASSERT_TRUE(send_tx);
            ASSERT_EQ(m_sender.count(), count - inps);
            ASSERT_EQ(send_tx.value().m_inputs.size(), inps);
            ASSERT_EQ(send_tx.value().m_outputs.size(), outs);
            m_sender.confirm_transaction(send_tx.value());
            ASSERT_EQ(m_sender.count(), count - inps + outs);
            ASSERT_EQ(m_sender.balance(), balance);
            count -= inps;
            count += outs;
        }
    }
}

TEST_F(WalletMultiTxTest, insufficient_utxos) {
    auto target_key = m_sender.generate_key();
    auto send_tx = m_sender.send_to(101, 5, target_key, true);
    ASSERT_EQ(send_tx, std::nullopt);
}

TEST_F(WalletMultiTxTest, too_many_outputs) {
    auto target_key = m_sender.generate_key();
    auto send_tx = m_sender.send_to(50, 10001, target_key, true);
    ASSERT_EQ(send_tx, std::nullopt);
}

TEST_F(WalletTest, spend_order) {
    m_wallet.confirm_transaction(m_wallet.mint_new_coins(1, 14));
    m_wallet.confirm_transaction(m_wallet.mint_new_coins(1, 22));
    m_wallet.confirm_transaction(m_wallet.mint_new_coins(1, 33));

    auto pubkey = m_wallet.generate_key();
    m_wallet.send_to(1, 1, pubkey, false);
    auto tx1 = m_wallet.send_to(1, 1, pubkey, false);
    ASSERT_TRUE(tx1.has_value());
    ASSERT_EQ(tx1->m_inputs[0].m_spend_data.value().m_value, 14UL);
    auto tx2 = m_wallet.send_to(1, pubkey, false);
    ASSERT_TRUE(tx2.has_value());
    ASSERT_EQ(tx2->m_inputs[0].m_spend_data.value().m_value, 22UL);
    auto tx3 = m_wallet.send_to(1, 1, pubkey, false);
    ASSERT_TRUE(tx3.has_value());
    ASSERT_EQ(tx3->m_inputs[0].m_spend_data.value().m_value, 33UL);
}

TEST_F(WalletTest, load_save) {
    m_wallet.save(m_wallet_file);
    auto new_wal = cbdc::transaction::wallet();
    new_wal.load(m_wallet_file);
    ASSERT_EQ(m_wallet.balance(), new_wal.balance());
    ASSERT_EQ(m_wallet.count(), new_wal.count());
}
