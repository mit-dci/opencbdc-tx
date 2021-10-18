// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/client/twophase_client.hpp"
#include "uhs/twophase/coordinator/controller.hpp"
#include "uhs/twophase/locking_shard/controller.hpp"
#include "uhs/twophase/sentinel_2pc/controller.hpp"
#include "util.hpp"
#include "util/network/socket.hpp"

#include <filesystem>
#include <gtest/gtest.h>

class two_phase_end_to_end_test : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::test::load_config(m_end_to_end_cfg_path, m_opts);

        m_wait_interval = std::chrono::milliseconds(1000);

        m_ctl_shard
            = std::make_unique<cbdc::locking_shard::controller>(0,
                                                                0,
                                                                m_opts,
                                                                m_logger);
        m_ctl_coordinator
            = std::make_unique<cbdc::coordinator::controller>(0,
                                                              0,
                                                              m_opts,
                                                              m_logger);
        m_ctl_sentinel
            = std::make_unique<cbdc::sentinel_2pc::controller>(0,
                                                               m_opts,
                                                               m_logger);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ASSERT_TRUE(m_ctl_shard->init());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_TRUE(m_ctl_coordinator->init());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_TRUE(m_ctl_sentinel->init());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        reload_sender();
        reload_receiver();

        std::this_thread::sleep_for(m_wait_interval);

        m_sender->mint(10, 10);
        std::this_thread::sleep_for(m_wait_interval);
        m_sender->sync();

        ASSERT_EQ(m_sender->balance(), 100UL);

        reload_sender();
    }

    void TearDown() override {
        m_sender.reset();
        m_receiver.reset();

        std::filesystem::remove(m_sender_wallet_store_file);
        std::filesystem::remove(m_sender_client_store_file);
        std::filesystem::remove(m_receiver_wallet_store_file);
        std::filesystem::remove(m_receiver_client_store_file);
        std::filesystem::remove_all("coordinator0_raft_log_0");
        std::filesystem::remove("coordinator0_raft_config_0.dat");
        std::filesystem::remove("coordinator0_raft_state_0.dat");
        std::filesystem::remove_all("shard0_raft_log_0");
        std::filesystem::remove("shard0_raft_config_0.dat");
        std::filesystem::remove("shard0_raft_state_0.dat");
        std::filesystem::remove("tp_samples.txt");
    }

    void reload_sender() {
        m_sender.reset();
        m_sender = std::make_unique<cbdc::twophase_client>(
            m_opts,
            m_logger,
            m_sender_wallet_store_file,
            m_sender_client_store_file);
        ASSERT_TRUE(m_sender->init());
    }

    void reload_receiver() {
        m_receiver.reset();
        m_receiver = std::make_unique<cbdc::twophase_client>(
            m_opts,
            m_logger,
            m_receiver_wallet_store_file,
            m_receiver_client_store_file);
        ASSERT_TRUE(m_receiver->init());
    }

    static constexpr auto m_end_to_end_cfg_path = "integration_tests_2pc.cfg";

    static constexpr auto m_sender_wallet_store_file = "s_wallet_store.dat";
    static constexpr auto m_sender_client_store_file = "s_client_store.dat";
    static constexpr auto m_receiver_wallet_store_file = "r_wallet_store.dat";
    static constexpr auto m_receiver_client_store_file = "r_client_store.dat";

    std::chrono::milliseconds m_wait_interval;

    cbdc::config::options m_opts{};

    std::shared_ptr<cbdc::logging::log> m_logger{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::trace)};

    std::unique_ptr<cbdc::locking_shard::controller> m_ctl_shard;
    std::unique_ptr<cbdc::coordinator::controller> m_ctl_coordinator;
    std::unique_ptr<cbdc::sentinel_2pc::controller> m_ctl_sentinel;

    std::unique_ptr<cbdc::twophase_client> m_sender;
    std::unique_ptr<cbdc::twophase_client> m_receiver;
};

TEST_F(two_phase_end_to_end_test, complete_transaction) {
    auto addr = m_receiver->new_address();

    auto [tx, res] = m_sender->send(33, addr);
    ASSERT_TRUE(tx.has_value());
    ASSERT_TRUE(res.has_value());
    ASSERT_FALSE(res->m_tx_error.has_value());
    ASSERT_EQ(res->m_tx_status, cbdc::sentinel::tx_status::confirmed);
    ASSERT_EQ(tx->m_outputs[0].m_value, 33UL);
    ASSERT_EQ(m_sender->balance(), 67UL);
    auto in = m_sender->export_send_inputs(tx.value(), addr);
    ASSERT_EQ(in.size(), 1UL);

    ASSERT_EQ(m_receiver->pending_input_count(), 0UL);
    m_receiver->import_send_input(in[0]);
    reload_receiver();
    ASSERT_EQ(m_receiver->balance(), 0UL);
    ASSERT_EQ(m_sender->pending_tx_count(), 0UL);
    ASSERT_EQ(m_receiver->pending_input_count(), 1UL);
    m_receiver->sync();
    ASSERT_EQ(m_receiver->balance(), 33UL);
    ASSERT_EQ(m_sender->pending_tx_count(), 0UL);
    ASSERT_EQ(m_receiver->pending_input_count(), 0UL);
}

TEST_F(two_phase_end_to_end_test, duplicate_transaction) {
    auto addr = m_receiver->new_address();

    auto [tx, res] = m_sender->send(33, addr);

    // Send the same transaction again
    auto res2 = m_sender->send_transaction(tx.value());

    ASSERT_TRUE(tx.has_value());
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(res2.has_value());
    ASSERT_FALSE(res->m_tx_error.has_value());
    ASSERT_FALSE(res2->m_tx_error.has_value());
    ASSERT_EQ(res->m_tx_status, cbdc::sentinel::tx_status::confirmed);
    ASSERT_EQ(res2->m_tx_status, cbdc::sentinel::tx_status::state_invalid);
    ASSERT_EQ(tx->m_outputs[0].m_value, 33UL);
    ASSERT_EQ(m_sender->balance(), 67UL);
    auto in = m_sender->export_send_inputs(tx.value(), addr);
    ASSERT_EQ(in.size(), 1UL);

    // Abandon the failed transaction
    auto abandoned
        = m_sender->abandon_transaction(cbdc::transaction::tx_id(tx.value()));
    ASSERT_TRUE(abandoned);

    ASSERT_EQ(m_receiver->pending_input_count(), 0UL);
    m_receiver->import_send_input(in[0]);
    reload_receiver();
    ASSERT_EQ(m_receiver->balance(), 0UL);
    ASSERT_EQ(m_sender->pending_tx_count(), 0UL);
    ASSERT_EQ(m_receiver->pending_input_count(), 1UL);
    m_receiver->sync();
    ASSERT_EQ(m_receiver->balance(), 33UL);
    ASSERT_EQ(m_sender->pending_tx_count(), 0UL);
    ASSERT_EQ(m_receiver->pending_input_count(), 0UL);
}

TEST_F(two_phase_end_to_end_test, double_spend_transaction) {
    auto addr = m_receiver->new_address();

    // Send the first transaction normally
    auto [tx, res] = m_sender->send(33, addr);

    ASSERT_TRUE(tx.has_value());
    ASSERT_TRUE(res.has_value());
    ASSERT_FALSE(res->m_tx_error.has_value());
    ASSERT_EQ(res->m_tx_status, cbdc::sentinel::tx_status::confirmed);
    ASSERT_EQ(tx->m_outputs[0].m_value, 33UL);
    ASSERT_EQ(m_sender->balance(), 67UL);
    auto in = m_sender->export_send_inputs(tx.value(), addr);
    ASSERT_EQ(in.size(), 1UL);
    ASSERT_EQ(m_receiver->pending_input_count(), 0UL);
    m_receiver->import_send_input(in[0]);
    reload_receiver();
    ASSERT_EQ(m_receiver->balance(), 0UL);
    ASSERT_EQ(m_receiver->pending_input_count(), 1UL);
    m_receiver->sync();
    ASSERT_EQ(m_receiver->balance(), 33UL);
    ASSERT_EQ(m_receiver->pending_input_count(), 0UL);

    // Create a second transaction
    auto tx2 = m_sender->create_transaction(33, addr);

    // Append the first input of the first transaction to this second
    // transaction, creating the double-spend. Also increase the output value
    // to balance the inputs and outputs
    tx2.value().m_inputs.push_back(tx.value().m_inputs[0]);
    tx2.value().m_outputs[0].m_value
        = tx2.value().m_outputs[0].m_value
        + tx.value().m_inputs[0].m_prevout_data.m_value;

    m_sender->sign_transaction(tx2.value());

    // Send the second transaction that double spends an input
    auto res2 = m_sender->send_transaction(tx2.value());
    ASSERT_TRUE(res2.has_value());
    ASSERT_FALSE(res2->m_tx_error.has_value());
    ASSERT_EQ(res2->m_tx_status, cbdc::sentinel::tx_status::state_invalid);

    // Check if the transaction is unconfirmed on the shard(s)
    auto res3 = m_sender->check_tx_id(cbdc::transaction::tx_id(tx2.value()));
    ASSERT_TRUE(res3.has_value());
    ASSERT_TRUE(res3.value());

    // Check if the outputs (excluding the appended double spend one) are
    // still marked as unspent on the shard
    for(size_t i = 0; i < tx2.value().m_inputs.size() - 1; i++) {
        auto inp = tx2.value().m_inputs[i];
        auto res4 = m_sender->check_unspent(inp.to_uhs_element().m_id);
        ASSERT_TRUE(res4.has_value());
        ASSERT_TRUE(res4.value());
    }

    // Abandon the failed transaction
    auto abandoned
        = m_sender->abandon_transaction(cbdc::transaction::tx_id(tx2.value()));
    ASSERT_TRUE(abandoned);

    // Confirm to see if our balance is restored after abandoning
    ASSERT_EQ(m_sender->balance(), 67UL);
}

TEST_F(two_phase_end_to_end_test, invalid_transaction) {
    auto addr = m_receiver->new_address();

    // Create the transaction normally
    auto tx = m_sender->create_transaction(33, addr);

    ASSERT_TRUE(tx.has_value());

    // Make transaction unbalanced
    tx.value().m_outputs[0].m_value = 1;
    // Sign it again
    m_sender->sign_transaction(tx.value());

    auto res = m_sender->send_transaction(tx.value());
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(res->m_tx_error.has_value());
    ASSERT_EQ(res->m_tx_status, cbdc::sentinel::tx_status::static_invalid);
    ASSERT_TRUE(
        std::holds_alternative<cbdc::transaction::validation::tx_error_code>(
            res->m_tx_error.value()));

    auto tx_err = std::get<cbdc::transaction::validation::tx_error_code>(
        res->m_tx_error.value());

    ASSERT_EQ(tx_err,
              cbdc::transaction::validation::tx_error_code::asymmetric_values);
}
