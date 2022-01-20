// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "archiver/controller.hpp"
#include "atomizer/controller.hpp"
#include "client/atomizer_client.hpp"
#include "mock_system.hpp"
#include "sentinel/controller.hpp"
#include "shard/controller.hpp"
#include "util.hpp"
#include "watchtower/controller.hpp"
#include "watchtower/tx_error_messages.hpp"

#include <filesystem>
#include <gtest/gtest.h>

class atomizer_end_to_end_test : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::test::load_config(m_end_to_end_cfg_path, m_opts);

        m_block_wait_interval = std::chrono::milliseconds(3000);

        m_ctl_watchtower
            = std::make_unique<cbdc::watchtower::controller>(0,
                                                             m_opts,
                                                             m_logger);
        m_ctl_atomizer
            = std::make_unique<cbdc::atomizer::controller>(0,
                                                           m_opts,
                                                           m_logger);

        m_ctl_archiver = std::make_unique<cbdc::archiver::controller>(0,
                                                                      m_opts,
                                                                      m_logger,
                                                                      0);
        m_ctl_shard
            = std::make_unique<cbdc::shard::controller>(0, m_opts, m_logger);
        m_ctl_sentinel
            = std::make_unique<cbdc::sentinel::controller>(0,
                                                           m_opts,
                                                           m_logger);

        std::thread w_init_thread([&]() {
            ASSERT_TRUE(m_ctl_watchtower->init());
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ASSERT_TRUE(m_ctl_atomizer->init());
        ASSERT_TRUE(m_ctl_archiver->init());
        ASSERT_TRUE(m_ctl_shard->init());
        ASSERT_TRUE(m_ctl_sentinel->init());

        reload_sender();
        reload_receiver();

        std::this_thread::sleep_for(m_block_wait_interval);

        m_sender->mint(10, 10);
        std::this_thread::sleep_for(m_block_wait_interval);
        m_sender->sync();

        reload_sender();

        w_init_thread.join();
    }

    void TearDown() override {
        m_sender = nullptr;
        m_receiver = nullptr;

        std::filesystem::remove_all("archiver0_db");
        std::filesystem::remove_all("atomizer_raft_log_0");
        std::filesystem::remove_all("atomizer_raft_config_0.dat");
        std::filesystem::remove_all("atomizer_raft_state_0.dat");
        std::filesystem::remove_all("atomizer_snps_0");
        std::filesystem::remove_all("shard0_db");
        std::filesystem::remove(m_sender_wallet_store_file);
        std::filesystem::remove(m_sender_client_store_file);
        std::filesystem::remove(m_receiver_wallet_store_file);
        std::filesystem::remove(m_receiver_client_store_file);
        std::filesystem::remove("tp_samples.txt");
    }

    void reload_sender() {
        m_sender = nullptr;
        m_sender = std::make_unique<cbdc::atomizer_client>(
            m_opts,
            m_logger,
            m_sender_wallet_store_file,
            m_sender_client_store_file);
        ASSERT_TRUE(m_sender->init());
    }

    void reload_receiver() {
        m_receiver = nullptr;
        m_receiver = std::make_unique<cbdc::atomizer_client>(
            m_opts,
            m_logger,
            m_receiver_wallet_store_file,
            m_receiver_client_store_file);
        ASSERT_TRUE(m_receiver->init());
    }

    static constexpr auto m_end_to_end_cfg_path = "integration_tests.cfg";

    static constexpr auto m_sender_wallet_store_file = "s_wallet_store.dat";
    static constexpr auto m_sender_client_store_file = "s_client_store.dat";
    static constexpr auto m_receiver_wallet_store_file = "r_wallet_store.dat";
    static constexpr auto m_receiver_client_store_file = "r_client_store.dat";

    std::chrono::milliseconds m_block_wait_interval;

    cbdc::config::options m_opts{};

    std::shared_ptr<cbdc::logging::log> m_logger{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::trace)};

    std::unique_ptr<cbdc::watchtower::controller> m_ctl_watchtower;
    std::unique_ptr<cbdc::atomizer::controller> m_ctl_atomizer;
    std::unique_ptr<cbdc::archiver::controller> m_ctl_archiver;
    std::unique_ptr<cbdc::shard::controller> m_ctl_shard;
    std::unique_ptr<cbdc::sentinel::controller> m_ctl_sentinel;

    std::unique_ptr<cbdc::client> m_sender;
    std::unique_ptr<cbdc::client> m_receiver;
};

TEST_F(atomizer_end_to_end_test, complete_transaction) {
    auto addr = m_receiver->new_address();

    auto [tx, res] = m_sender->send(33, addr);
    ASSERT_TRUE(tx.has_value());
    ASSERT_TRUE(res.has_value());
    ASSERT_FALSE(res->m_tx_error.has_value());
    ASSERT_EQ(res->m_tx_status, cbdc::sentinel::tx_status::pending);
    ASSERT_EQ(tx->m_outputs[0].m_value, 33);
    ASSERT_EQ(m_sender->balance(), 60);
    auto in = m_sender->export_send_inputs(tx.value(), addr);
    ASSERT_EQ(in.size(), 1);

    std::this_thread::sleep_for(m_block_wait_interval);
    reload_sender();
    ASSERT_EQ(m_sender->balance(), 60);
    ASSERT_EQ(m_sender->pending_tx_count(), 1);
    ASSERT_EQ(m_sender->pending_input_count(), 0);
    m_sender->sync();
    ASSERT_EQ(m_sender->balance(), 67);
    ASSERT_EQ(m_sender->pending_tx_count(), 0);

    ASSERT_EQ(m_receiver->pending_input_count(), 0);
    m_receiver->import_send_input(in[0]);
    reload_receiver();
    ASSERT_EQ(m_receiver->balance(), 0);
    ASSERT_EQ(m_sender->pending_tx_count(), 0);
    ASSERT_EQ(m_receiver->pending_input_count(), 1);
    m_receiver->sync();
    ASSERT_EQ(m_receiver->balance(), 33);
    ASSERT_EQ(m_sender->pending_tx_count(), 0);
    ASSERT_EQ(m_receiver->pending_input_count(), 0);
}

TEST_F(atomizer_end_to_end_test, double_spend) {
    auto addr = m_receiver->new_address();

    auto wc = cbdc::watchtower::blocking_client(
        m_opts.m_watchtower_client_endpoints[0]);
    ASSERT_TRUE(wc.init());
    auto bbh = wc.request_best_block_height()->height() + 1;
    auto [tx, res] = m_sender->send(33, addr);
    ASSERT_TRUE(tx.has_value());
    ASSERT_TRUE(res.has_value());
    ASSERT_FALSE(res->m_tx_error.has_value());
    ASSERT_EQ(res->m_tx_status, cbdc::sentinel::tx_status::pending);
    ASSERT_EQ(tx->m_outputs[0].m_value, 33);
    ASSERT_EQ(m_sender->balance(), 60);

    std::this_thread::sleep_for(m_block_wait_interval);
    reload_sender();
    ASSERT_EQ(m_sender->balance(), 60);
    ASSERT_EQ(m_sender->pending_tx_count(), 1);
    ASSERT_EQ(m_sender->pending_input_count(), 0);
    m_sender->sync();
    ASSERT_EQ(m_sender->balance(), 67);
    ASSERT_EQ(m_sender->pending_tx_count(), 0);

    auto sc
        = cbdc::sentinel::rpc::client(m_opts.m_sentinel_endpoints, m_logger);
    ASSERT_TRUE(sc.init());
    res = sc.execute_transaction(tx.value());
    ASSERT_TRUE(res.has_value());
    ASSERT_FALSE(res->m_tx_error.has_value());
    ASSERT_EQ(res->m_tx_status, cbdc::sentinel::tx_status::pending);

    std::this_thread::sleep_for(m_block_wait_interval);

    auto ctx = cbdc::transaction::compact_tx(tx.value());
    auto wc_res = wc.request_status_update(
        cbdc::watchtower::status_update_request{{{ctx.m_id,
                                                  {
                                                      ctx.m_inputs[0],
                                                      ctx.m_inputs[1],
                                                      ctx.m_uhs_outputs[0],
                                                      ctx.m_uhs_outputs[1],
                                                  }}}});

    auto want = cbdc::watchtower::status_request_check_success{
        {{ctx.m_id,
          {cbdc::watchtower::status_update_state{
               cbdc::watchtower::search_status::spent,
               bbh,
               ctx.m_inputs[0]},
           cbdc::watchtower::status_update_state{
               cbdc::watchtower::search_status::spent,
               bbh,
               ctx.m_inputs[1]},
           cbdc::watchtower::status_update_state{
               cbdc::watchtower::search_status::unspent,
               bbh,
               ctx.m_uhs_outputs[0]},
           cbdc::watchtower::status_update_state{
               cbdc::watchtower::search_status::unspent,
               bbh,
               ctx.m_uhs_outputs[1]}}}}};
    ASSERT_EQ(*wc_res, want);
}

TEST_F(atomizer_end_to_end_test, invalid_transaction) {
    auto addr = m_receiver->new_address();
    auto wc = cbdc::watchtower::blocking_client(
        m_opts.m_watchtower_client_endpoints[0]);
    ASSERT_TRUE(wc.init());
    auto tx = m_sender->create_transaction(33, addr);
    ASSERT_TRUE(tx.has_value());

    tx.value().m_outputs[0].m_value = 1; // Unbalanced

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

    std::this_thread::sleep_for(m_block_wait_interval);

    auto bbh = wc.request_best_block_height()->height();
    auto ctx = cbdc::transaction::compact_tx(tx.value());
    auto wc_res = wc.request_status_update(
        cbdc::watchtower::status_update_request{{{ctx.m_id,
                                                  {
                                                      ctx.m_inputs[0],
                                                      ctx.m_inputs[1],
                                                      ctx.m_uhs_outputs[0],
                                                      ctx.m_uhs_outputs[1],
                                                  }}}});

    auto want = cbdc::watchtower::status_request_check_success{
        {{ctx.m_id,
          {cbdc::watchtower::status_update_state{
               cbdc::watchtower::search_status::no_history,
               bbh,
               ctx.m_inputs[0]},
           cbdc::watchtower::status_update_state{
               cbdc::watchtower::search_status::no_history,
               bbh,
               ctx.m_inputs[1]},
           cbdc::watchtower::status_update_state{
               cbdc::watchtower::search_status::no_history,
               bbh,
               ctx.m_uhs_outputs[0]},
           cbdc::watchtower::status_update_state{
               cbdc::watchtower::search_status::no_history,
               bbh,
               ctx.m_uhs_outputs[1]}}}}};

    ASSERT_EQ(*wc_res, want);
}
