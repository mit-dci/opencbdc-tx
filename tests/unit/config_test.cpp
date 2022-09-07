// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.hpp"
#include "util/common/config.hpp"

#include <gtest/gtest.h>
#include <string>

// TODO: Add file parsing tests.

class config_validation_test : public ::testing::Test {
  protected:
    void SetUp() override {
        m_atomizer_opts.m_twophase_mode = false;
        m_atomizer_opts.m_atomizer_endpoints.emplace_back();
        m_atomizer_opts.m_archiver_endpoints.emplace_back();
        m_atomizer_opts.m_watchtower_client_endpoints.emplace_back();
        m_atomizer_opts.m_sentinel_endpoints.emplace_back();
        m_atomizer_opts.m_sentinel_public_keys.emplace();
        m_atomizer_opts.m_shard_endpoints.emplace_back();

        m_twophase_opts.m_twophase_mode = true;
        m_twophase_opts.m_sentinel_endpoints.emplace_back();
        m_twophase_opts.m_sentinel_public_keys.emplace();
        m_twophase_opts.m_locking_shard_endpoints.emplace_back();
        m_twophase_opts.m_coordinator_endpoints.emplace_back();

        m_example_config = "archiver0_endpoint=\"127.0.0.1:5558\"\n"
                           "archiver0_db=\"ex_db\"\n"
                           "window_size=40000\n"
                           "shard0_loglevel=\"WARN\"\n"
                           "loadgen_invalid_tx_rate=13.00\n";
    }

    cbdc::config::options m_atomizer_opts;
    cbdc::config::options m_twophase_opts;
    std::string m_example_config;
};

TEST_F(config_validation_test, valid_config_atomizer_no_sentinels) {
    m_atomizer_opts.m_sentinel_endpoints.clear();
    auto err = cbdc::config::check_options(m_atomizer_opts);
    ASSERT_FALSE(err.has_value());
}

TEST_F(config_validation_test, valid_config_atomizer_sentinels) {
    auto err = cbdc::config::check_options(m_atomizer_opts);
    ASSERT_FALSE(err.has_value());
}

TEST_F(config_validation_test, watchtowers_invariant) {
    m_atomizer_opts.m_watchtower_client_endpoints.clear();
    auto err = cbdc::config::check_options(m_atomizer_opts);
    ASSERT_TRUE(err.has_value());
}

TEST_F(config_validation_test, atomizer_invariant) {
    m_atomizer_opts.m_atomizer_endpoints.clear();
    auto err = cbdc::config::check_options(m_atomizer_opts);
    ASSERT_TRUE(err.has_value());
}

TEST_F(config_validation_test, archiver_invariant) {
    m_atomizer_opts.m_archiver_endpoints.clear();
    auto err = cbdc::config::check_options(m_atomizer_opts);
    ASSERT_TRUE(err.has_value());
}

TEST_F(config_validation_test, valid_config_twophase) {
    auto err = cbdc::config::check_options(m_twophase_opts);
    ASSERT_FALSE(err.has_value());
}

TEST_F(config_validation_test, twophase_sentinel_invariant) {
    m_twophase_opts.m_sentinel_endpoints.clear();
    auto err = cbdc::config::check_options(m_twophase_opts);
    ASSERT_TRUE(err.has_value());
}

TEST_F(config_validation_test, twophase_shard_invariant) {
    m_twophase_opts.m_locking_shard_endpoints.clear();
    auto err = cbdc::config::check_options(m_twophase_opts);
    ASSERT_TRUE(err.has_value());
}

TEST_F(config_validation_test, twophase_coordinator_invariant) {
    m_twophase_opts.m_coordinator_endpoints.clear();
    auto err = cbdc::config::check_options(m_twophase_opts);
    ASSERT_TRUE(err.has_value());
}

TEST_F(config_validation_test, parsing_validation) {
    std::istringstream cfg(m_example_config);
    cbdc::config::parser ex(cfg);

    auto window_size = ex.get_ulong("window_size");
    EXPECT_TRUE(window_size.has_value());
    EXPECT_EQ(window_size.value(), 40000UL);

    auto endpoint = ex.get_endpoint("archiver0_endpoint");
    EXPECT_TRUE(endpoint.has_value());

    auto& [host, port] = endpoint.value();
    EXPECT_EQ(host, "127.0.0.1");
    EXPECT_EQ(port, 5558);

    auto db = ex.get_string("archiver0_db");
    EXPECT_TRUE(db.has_value());
    EXPECT_EQ(db.value(), "ex_db");

    auto loglevel = ex.get_loglevel("shard0_loglevel");
    EXPECT_TRUE(loglevel.has_value());
    EXPECT_EQ(loglevel.value(), cbdc::logging::log_level::warn);

    auto decimal = ex.get_decimal("loadgen_invalid_tx_rate");
    EXPECT_TRUE(decimal.has_value());
    EXPECT_EQ(decimal.value(), 13.0);

    auto nonexistent = ex.get_string("lorem ipsum");
    EXPECT_FALSE(nonexistent.has_value());
}
