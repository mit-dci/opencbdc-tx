// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mock_system.hpp"
#include "shard/controller.hpp"
#include "transaction/messages.hpp"
#include "transaction/wallet.hpp"
#include "util.hpp"
#include "watchtower/tx_error_messages.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <vector>

class replicated_shard_integration_tests : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::test::load_config(m_shard_cfg_path, m_opts);
        m_shard_count = m_opts.m_shard_endpoints.size();
        for(auto i = 0ULL; i < m_shard_count; ++i) {
            m_ctls.push_back(
                std::make_unique<cbdc::shard::controller>(i,
                                                          m_opts,
                                                          m_logger));
        }
        auto ignore_set = std::unordered_set<cbdc::test::mock_system_module>(
            {cbdc::test::mock_system_module::shard});
        m_sys = std::make_unique<cbdc::test::mock_system>(ignore_set, m_opts);
        m_sys->init();
        for(auto& ctl : m_ctls) {
            ASSERT_TRUE(ctl->init());
        }

        ASSERT_TRUE(m_client.connect(m_opts.m_shard_endpoints));
    }

    void TearDown() override {
        for(const auto& dir : m_opts.m_shard_db_dirs) {
            std::filesystem::remove_all(dir);
        }
    }

    static constexpr auto m_shard_cfg_path = "replicated_shard.cfg";

    size_t m_shard_count;

    cbdc::config::options m_opts{};

    std::unique_ptr<cbdc::test::mock_system> m_sys;

    std::shared_ptr<cbdc::logging::log> m_logger{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::trace)};

    std::vector<std::unique_ptr<cbdc::shard::controller>> m_ctls;
    cbdc::test::simple_client<std::nullopt_t> m_client;
};

TEST_F(replicated_shard_integration_tests,
       can_send_messages_from_multiple_shards) {
    auto mint_tx = cbdc::test::simple_tx({'a'}, {}, {{'c'}});
    auto init_blk = cbdc::atomizer::block{1, {mint_tx}};

    auto br = m_sys->broadcast_from<cbdc::atomizer::block>(
        cbdc::test::mock_system_module::atomizer,
        init_blk);
    ASSERT_TRUE(br);

    std::this_thread::sleep_for(std::chrono::seconds(5));

    auto spend_tx = cbdc::test::simple_tx({'d'}, {{'c'}}, {{'e'}});

    std::vector<std::future<cbdc::atomizer::tx_notify_message>> res;
    res.reserve(m_shard_count);
    for(auto i = 0ULL; i < m_shard_count; ++i) {
        res.push_back(m_sys->expect<cbdc::atomizer::tx_notify_message>(
            cbdc::test::mock_system_module::atomizer));
    }

    m_client.broadcast(spend_tx);

    for(auto& fut : res) {
        auto status = fut.wait_for(std::chrono::seconds(1));
        ASSERT_EQ(status, std::future_status::ready);
        auto msg = fut.get();
        ASSERT_EQ(msg.m_tx, spend_tx);
    }
}
