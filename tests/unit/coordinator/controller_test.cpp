// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/twophase/coordinator/controller.hpp"
#include "util.hpp"

#include <gtest/gtest.h>

class coordinator_controller_test : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::test::load_config(cfg_path, m_opts);

        m_logger = std::make_shared<cbdc::logging::log>(
            cbdc::logging::log_level::debug);
    }

    void TearDown() override {
        std::filesystem::remove_all("coordinator0_raft_log_0");
        std::filesystem::remove("coordinator0_raft_config_0.dat");
        std::filesystem::remove("coordinator0_raft_state_0.dat");
    }

    static constexpr auto cfg_path = "coordinator.cfg";
    std::unique_ptr<cbdc::coordinator::controller> m_ctl_coordinator;
    cbdc::config::options m_opts{};
    std::shared_ptr<cbdc::logging::log> m_logger;
};

TEST_F(coordinator_controller_test, no_logger) {
    std::shared_ptr<cbdc::logging::log> logger;
    m_ctl_coordinator
        = std::make_unique<cbdc::coordinator::controller>(0,
                                                          0,
                                                          m_opts,
                                                          logger);
    ASSERT_FALSE(m_ctl_coordinator->init());
}

TEST_F(coordinator_controller_test, out_of_range_shard_id) {
    m_ctl_coordinator
        = std::make_unique<cbdc::coordinator::controller>(1,
                                                          0,
                                                          m_opts,
                                                          m_logger);
    ASSERT_FALSE(m_ctl_coordinator->init());
}

TEST_F(coordinator_controller_test, out_of_range_node_id) {
    m_ctl_coordinator
        = std::make_unique<cbdc::coordinator::controller>(0,
                                                          1,
                                                          m_opts,
                                                          m_logger);
    ASSERT_FALSE(m_ctl_coordinator->init());
}

TEST_F(coordinator_controller_test, successful_init) {
    m_ctl_coordinator
        = std::make_unique<cbdc::coordinator::controller>(0,
                                                          0,
                                                          m_opts,
                                                          m_logger);
    ASSERT_TRUE(m_ctl_coordinator->init());
}
