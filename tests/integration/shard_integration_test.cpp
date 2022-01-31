// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mock_system.hpp"
#include "uhs/atomizer/shard/controller.hpp"
#include "uhs/atomizer/watchtower/tx_error_messages.hpp"
#include "uhs/transaction/messages.hpp"
#include "util.hpp"

#include <filesystem>
#include <gtest/gtest.h>

class shard_integration_test : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::test::load_config(m_shard_cfg_path, m_opts);
        m_ctl = std::make_unique<cbdc::shard::controller>(0, m_opts, m_logger);
        auto ignore_set = std::unordered_set<cbdc::test::mock_system_module>(
            {cbdc::test::mock_system_module::shard});
        m_sys = std::make_unique<cbdc::test::mock_system>(ignore_set, m_opts);
        m_sys->init();
        ASSERT_TRUE(m_ctl->init());
        ASSERT_TRUE(m_client.connect(m_opts.m_shard_endpoints));
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    void TearDown() override {
        std::filesystem::remove_all("shard0_db");
    }

    static constexpr auto m_shard_cfg_path = "integration_tests.cfg";

    cbdc::config::options m_opts{};

    std::unique_ptr<cbdc::test::mock_system> m_sys;

    std::shared_ptr<cbdc::logging::log> m_logger{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::trace)};

    std::unique_ptr<cbdc::shard::controller> m_ctl;
    cbdc::test::simple_client<std::nullopt_t> m_client;
};

TEST_F(shard_integration_test, error_non_existant_input) {
    auto got_err = m_sys->expect<std::vector<cbdc::watchtower::tx_error>>(
        cbdc::test::mock_system_module::watchtower);

    auto init_blk = cbdc::atomizer::block{1, {}};

    ASSERT_TRUE(m_sys->broadcast_from<cbdc::atomizer::block>(
        cbdc::test::mock_system_module::atomizer,
        init_blk));
    std::this_thread::sleep_for(std::chrono::seconds(1));

    m_client.broadcast(cbdc::test::simple_tx({'a'}, {{'b'}}, {{'c'}}));

    auto status = got_err.wait_for(std::chrono::seconds(1));
    ASSERT_EQ(status, std::future_status::ready);
    std::vector<cbdc::watchtower::tx_error> want{cbdc::watchtower::tx_error{
        {'a'},
        cbdc::watchtower::tx_error_inputs_dne{{{'b'}}}}};
    ASSERT_EQ(got_err.get(), want);
}

TEST_F(shard_integration_test, error_initial_sync) {
    auto got_err = m_sys->expect<std::vector<cbdc::watchtower::tx_error>>(
        cbdc::test::mock_system_module::watchtower);

    m_client.broadcast(cbdc::test::simple_tx({'a'}, {{'b'}}, {{'c'}}));

    auto status = got_err.wait_for(std::chrono::seconds(1));
    ASSERT_EQ(status, std::future_status::ready);
    std::vector<cbdc::watchtower::tx_error> want{
        cbdc::watchtower::tx_error{{'a'}, cbdc::watchtower::tx_error_sync{}}};
    ASSERT_EQ(got_err.get(), want);
}
