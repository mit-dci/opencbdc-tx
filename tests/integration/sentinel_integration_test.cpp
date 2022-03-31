// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mock_system.hpp"
#include "uhs/atomizer/sentinel/controller.hpp"
#include "uhs/sentinel/client.hpp"
#include "uhs/sentinel/format.hpp"
#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/wallet.hpp"
#include "util.hpp"

#include <gtest/gtest.h>

class sentinel_integration_test : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::test::load_config(m_sentinel_cfg_path, m_opts);
        m_ctl = std::make_unique<cbdc::sentinel::controller>(0,
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

    static constexpr auto m_sentinel_cfg_path = "integration_tests.cfg";

    cbdc::config::options m_opts{};

    std::unique_ptr<cbdc::test::mock_system> m_sys;

    std::shared_ptr<cbdc::logging::log> m_logger{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::trace)};

    std::unique_ptr<cbdc::sentinel::controller> m_ctl;
    std::unique_ptr<cbdc::sentinel::rpc::client> m_client;
};

// TODO: Redo, removing dependence on Wallet.
TEST_F(sentinel_integration_test, valid_signed_tx) {
    cbdc::transaction::wallet wallet;
    cbdc::transaction::full_tx m_valid_tx{};

    auto mint_tx1 = wallet.mint_new_coins(2, 100);
    wallet.confirm_transaction(mint_tx1);

    auto tx = wallet.send_to(2, 2, wallet.generate_key(), true);
    ASSERT_TRUE(tx.has_value());

    auto err = m_sys->expect<cbdc::transaction::compact_tx>(
        cbdc::test::mock_system_module::shard);

    auto ctx = cbdc::transaction::compact_tx(tx.value());
    cbdc::sentinel::execute_response want{cbdc::sentinel::tx_status::pending,
                                          std::nullopt};
    auto got = m_client->execute_transaction(tx.value());
    ASSERT_TRUE(got.has_value());
    cbdc::test::print_sentinel_error(got->m_tx_error);
    ASSERT_EQ(got.value(), want);
}
