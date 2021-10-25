// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mock_system.hpp"
#include "uhs/sentinel/client.hpp"
#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/wallet.hpp"
#include "uhs/twophase/coordinator/format.hpp"
#include "uhs/twophase/sentinel_2pc/controller.hpp"
#include "util.hpp"
#include "util/rpc/format.hpp"
#include "util/serialization/util.hpp"

#include <gtest/gtest.h>

class sentinel_2pc_integration_test : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::test::load_config(m_sentinel_cfg_path, m_opts);
        m_ctl = std::make_unique<cbdc::sentinel_2pc::controller>(0,
                                                                 m_opts,
                                                                 m_logger);
        auto ignore_set = std::unordered_set<cbdc::test::mock_system_module>(
            {cbdc::test::mock_system_module::sentinel});
        m_sys = std::make_unique<cbdc::test::mock_system>(ignore_set, m_opts);
        m_sys->init();
        ASSERT_TRUE(m_ctl->init());
        m_client = std::make_unique<cbdc::sentinel::rpc::client>(
            m_opts.m_sentinel_endpoints,
            m_logger);
        ASSERT_TRUE(m_client->init());
    }

    static constexpr auto m_sentinel_cfg_path = "integration_tests_2pc.cfg";

    cbdc::config::options m_opts{};

    std::unique_ptr<cbdc::test::mock_system> m_sys;

    std::shared_ptr<cbdc::logging::log> m_logger{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::trace)};

    std::unique_ptr<cbdc::sentinel_2pc::controller> m_ctl;
    std::unique_ptr<cbdc::sentinel::rpc::client> m_client;
};

// TODO: Redo, removing dependence on Wallet.
TEST_F(sentinel_2pc_integration_test, valid_signed_tx) {
    cbdc::transaction::wallet wallet;
    cbdc::transaction::full_tx m_valid_tx{};

    auto mint_tx1 = wallet.mint_new_coins(2, 100);
    wallet.confirm_transaction(mint_tx1);

    auto tx = wallet.send_to(2, 2, wallet.generate_key(), true);
    ASSERT_TRUE(tx.has_value());

    auto ctx = cbdc::transaction::compact_tx(tx.value());

    auto err = m_sys->expect<cbdc::transaction::compact_tx>(
        cbdc::test::mock_system_module::coordinator);

    cbdc::sentinel::execute_response want{cbdc::sentinel::tx_status::confirmed,
                                          std::nullopt};

    std::thread client_thread([&]() {
        auto got = m_client->execute_transaction(tx.value());
        ASSERT_TRUE(got.has_value());
        cbdc::test::print_sentinel_error(got->m_tx_error);
        ASSERT_EQ(got.value(), want);
    });

    // Wait for the sentinel client message to send.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto pkt = std::make_shared<cbdc::buffer>(cbdc::make_buffer(
        cbdc::rpc::response<cbdc::coordinator::rpc::response>{{0}, true}));

    ASSERT_TRUE(
        m_sys->broadcast_from(cbdc::test::mock_system_module::coordinator,
                              pkt));

    client_thread.join();
}
