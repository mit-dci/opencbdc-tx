// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/twophase/locking_shard/controller.hpp"
#include "util.hpp"

#include <gtest/gtest.h>

class locking_shard_test : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::test::load_config(cfg_path, m_opts);

        m_logger = std::make_shared<cbdc::logging::log>(
            cbdc::logging::log_level::debug);
    }

    void TearDown() override {
        std::filesystem::remove_all("shard0_raft_log_0");
        std::filesystem::remove("shard0_raft_config_0.dat");
        std::filesystem::remove("shard0_raft_state_0.dat");
    }

    static constexpr auto cfg_path = "locking_shard.cfg";
    std::unique_ptr<cbdc::locking_shard::controller> m_ctl_shard;
    cbdc::config::options m_opts{};
    std::shared_ptr<cbdc::logging::log> m_logger;
};

TEST_F(locking_shard_test, no_logger) {
    std::shared_ptr<cbdc::logging::log> logger;
    m_ctl_shard = std::make_unique<cbdc::locking_shard::controller>(0,
                                                                    0,
                                                                    m_opts,
                                                                    logger);
    ASSERT_FALSE(m_ctl_shard->init());
}

TEST_F(locking_shard_test, out_of_range_shard_id) {
    m_ctl_shard = std::make_unique<cbdc::locking_shard::controller>(1,
                                                                    0,
                                                                    m_opts,
                                                                    m_logger);
    ASSERT_FALSE(m_ctl_shard->init());
}

TEST_F(locking_shard_test, out_of_range_node_id) {
    m_ctl_shard = std::make_unique<cbdc::locking_shard::controller>(0,
                                                                    1,
                                                                    m_opts,
                                                                    m_logger);
    ASSERT_FALSE(m_ctl_shard->init());
}

TEST_F(locking_shard_test, successful_init) {
    m_ctl_shard = std::make_unique<cbdc::locking_shard::controller>(0,
                                                                    0,
                                                                    m_opts,
                                                                    m_logger);
    ASSERT_TRUE(m_ctl_shard->init());
}
