// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mock_system.hpp"
#include "uhs/atomizer/atomizer/format.hpp"
#include "uhs/atomizer/watchtower/client.hpp"
#include "uhs/atomizer/watchtower/controller.hpp"
#include "uhs/atomizer/watchtower/tx_error_messages.hpp"
#include "util.hpp"

#include <filesystem>
#include <gtest/gtest.h>

class watchtower_integration_test : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::test::load_config(m_watchtower_cfg_path, m_opts);
        m_ctl = std::make_unique<cbdc::watchtower::controller>(0,
                                                               m_opts,
                                                               m_logger);
        auto ignore_set = std::unordered_set<cbdc::test::mock_system_module>(
            {cbdc::test::mock_system_module::watchtower});
        m_sys = std::make_unique<cbdc::test::mock_system>(ignore_set, m_opts);

        m_sys->init();
        ASSERT_TRUE(m_ctl->init());

        m_wc = std::make_unique<cbdc::watchtower::blocking_client>(
            m_opts.m_watchtower_client_endpoints[0]);
        ASSERT_TRUE(m_wc->init());

        ASSERT_TRUE(
            m_watchtower_internal_client.connect(m_opts.m_sentinel_endpoints));

        // Wait for the networks to finish accepting the watchtower's
        // connections.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        cbdc::atomizer::block b0;
        b0.m_height = m_best_height;
        b0.m_transactions.push_back(
            cbdc::test::simple_tx({'t', 'x', 'a'},
                                  {{'s', 'b'}, {'s', 'c'}},
                                  {{'u', 'd'}}));

        auto pkt = std::make_shared<cbdc::buffer>();
        auto ser = cbdc::buffer_serializer(*pkt);
        ser << b0;
        ASSERT_TRUE(
            m_sys->broadcast_from(cbdc::test::mock_system_module::atomizer,
                                  pkt));

        // Wait for the watchtower to receive and digest the first atomizer
        // block.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto res = m_wc->request_best_block_height();
        ASSERT_EQ(res->height(), m_best_height);
    }

    static constexpr auto m_watchtower_cfg_path = "integration_tests.cfg";

    static constexpr auto m_best_height{1};

    cbdc::config::options m_opts{};

    std::unique_ptr<cbdc::test::mock_system> m_sys;

    std::shared_ptr<cbdc::logging::log> m_logger{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::trace)};

    std::unique_ptr<cbdc::watchtower::controller> m_ctl;
    std::unique_ptr<cbdc::watchtower::blocking_client> m_wc;
    cbdc::test::simple_client<std::nullopt_t> m_watchtower_internal_client;
};

TEST_F(watchtower_integration_test, check_spent_unspent) {
    auto got
        = m_wc->request_status_update(cbdc::watchtower::status_update_request{
            {{{'t', 'x', 'a'}, {{'s', 'b'}, {'u', 'd'}}}}});

    auto want = cbdc::watchtower::status_request_check_success{
        {{{'t', 'x', 'a'},
          {cbdc::watchtower::status_update_state{
               cbdc::watchtower::search_status::spent,
               m_best_height,
               {'s', 'b'}},
           cbdc::watchtower::status_update_state{
               cbdc::watchtower::search_status::unspent,
               m_best_height,
               {'u', 'd'}}}}}};
    ASSERT_EQ(*got, want);
}

TEST_F(watchtower_integration_test, check_no_data) {
    auto got
        = m_wc->request_status_update(cbdc::watchtower::status_update_request{
            {{{'t', 'x', 'z'}, {{'u', 'y'}}}}});

    auto want = cbdc::watchtower::status_request_check_success{
        {{{'t', 'x', 'z'},
          {cbdc::watchtower::status_update_state{
              cbdc::watchtower::search_status::no_history,
              m_best_height,
              {'u', 'y'}}}}}};
    ASSERT_EQ(*got, want);
}

// TODO: test async_client.
