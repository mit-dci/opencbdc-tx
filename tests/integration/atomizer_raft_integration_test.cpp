// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mock_system.hpp"
#include "uhs/atomizer/atomizer/atomizer_raft.hpp"
#include "uhs/atomizer/atomizer/controller.hpp"
#include "uhs/atomizer/atomizer/format.hpp"
#include "uhs/atomizer/watchtower/tx_error_messages.hpp"
#include "util.hpp"

#include <filesystem>
#include <gtest/gtest.h>

// Note the use of `expect_block` to read block messages from the atomizer
// (rather than `expect`); this is because the atomizer creates blocks
// at a regular interval, and so does not send these in messages as is done
// with other communications.
class atomizer_raft_integration_test : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::test::load_config(m_shard_cfg_path, m_opts);
        m_opts.m_attestation_threshold = 0;
        m_ctl = std::make_unique<cbdc::atomizer::controller>(0,
                                                             m_opts,
                                                             m_logger);
        auto ignore_set = std::unordered_set<cbdc::test::mock_system_module>(
            {cbdc::test::mock_system_module::atomizer});
        m_sys = std::make_unique<cbdc::test::mock_system>(ignore_set, m_opts);

        m_sys->init();
        ASSERT_TRUE(m_ctl->init());
        m_conn.connect(m_opts.m_atomizer_endpoints[0]);

        auto bct = m_block_net.start_cluster_handler(
            {m_opts.m_atomizer_endpoints[0]},
            [&](cbdc::network::message_t&& pkt)
                -> std::optional<cbdc::buffer> {
                cbdc::test::block res;
                auto deser = cbdc::buffer_serializer(*pkt.m_pkt);
                deser >> res;
                {
                    std::unique_lock lk{m_bm};
                    m_received_blocks.insert({res.m_height, res});
                }
                m_bcv.notify_all();
                return std::nullopt;
            });

        ASSERT_TRUE(bct.has_value());
        m_block_client_thread = std::move(bct.value());
    }

    void TearDown() override {
        m_conn.disconnect();
        m_block_net.close();

        if(m_block_client_thread.joinable()) {
            m_block_client_thread.join();
        }

        std::filesystem::remove_all("archiver0_db");
        std::filesystem::remove_all("atomizer_raft_log_0");
        std::filesystem::remove_all("atomizer_raft_config_0.dat");
        std::filesystem::remove_all("atomizer_raft_state_0.dat");
        std::filesystem::remove_all("atomizer_snps_0");
    }

    void expect_block(const cbdc::atomizer::block& blk,
                      const std::chrono::seconds& timeout
                      = std::chrono::seconds(5)) {
        std::unique_lock lk{m_bm};
        bool match{true};
        auto res = m_bcv.wait_for(lk, timeout, [&] {
            auto it = m_received_blocks.find(blk.m_height);
            if(it == m_received_blocks.end()) {
                return false;
            }
            match = it->second == blk;
            return match;
        });
        if(!match) {
            ADD_FAILURE() << "Expected block does not match received block";
        } else if(!res) {
            ADD_FAILURE() << "Block never received.";
        }
    }

    static constexpr auto m_shard_cfg_path = "integration_tests.cfg";

    cbdc::config::options m_opts{};

    std::unique_ptr<cbdc::test::mock_system> m_sys;

    std::shared_ptr<cbdc::logging::log> m_logger{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::trace)};

    std::unique_ptr<cbdc::atomizer::controller> m_ctl;
    cbdc::network::tcp_socket m_conn;

    cbdc::network::connection_manager m_block_net;
    std::thread m_block_client_thread;
    std::unordered_map<uint32_t, cbdc::test::block> m_received_blocks;
    std::mutex m_bm;
    std::condition_variable m_bcv;
};

TEST_F(atomizer_raft_integration_test, basic) {
    ASSERT_TRUE(
        m_conn.send(cbdc::atomizer::request{cbdc::atomizer::tx_notify_request{
            cbdc::test::simple_tx({'a'},
                                  {{'b'}, {'c'}},
                                  {{{'d'}, {'e'}, {'f'}}}),
            {0, 1},
            0}}));

    ASSERT_TRUE(
        m_conn.send(cbdc::atomizer::request{cbdc::atomizer::tx_notify_request{
            cbdc::test::simple_tx({'e'},
                                  {{'f'}, {'g'}},
                                  {{{'h'}, {'i'}, {'j'}}}),
            {0, 1},
            0}}));

    cbdc::test::block want_block;
    want_block.m_height = 1;
    want_block.m_transactions.push_back(
        cbdc::test::simple_tx({'a'}, {{'b'}, {'c'}}, {{{'d'}, {'e'}, {'f'}}}));
    want_block.m_transactions.push_back(
        cbdc::test::simple_tx({'e'}, {{'f'}, {'g'}}, {{{'h'}, {'i'}, {'j'}}}));
    expect_block(want_block);
}

TEST_F(atomizer_raft_integration_test, error_inputs_spent) {
    auto got_err = m_sys->expect<std::vector<cbdc::watchtower::tx_error>>(
        cbdc::test::mock_system_module::watchtower);

    ASSERT_TRUE(
        m_conn.send(cbdc::atomizer::request{cbdc::atomizer::tx_notify_request{
            cbdc::test::simple_tx({'a'},
                                  {{'B'}, {'c'}},
                                  {{{'d'}, {'e'}, {'f'}}}),
            {0, 1},
            0}}));

    ASSERT_TRUE(
        m_conn.send(cbdc::atomizer::request{cbdc::atomizer::tx_notify_request{
            cbdc::test::simple_tx({'E'},
                                  {{'B'}, {'f'}},
                                  {{{'g'}, {'h'}, {'i'}}}),
            {0, 1},
            0}}));

    auto status = got_err.wait_for(std::chrono::seconds(10));
    ASSERT_EQ(status, std::future_status::ready);
    std::vector<cbdc::watchtower::tx_error> want{cbdc::watchtower::tx_error{
        {'E'},
        cbdc::watchtower::tx_error_inputs_spent{{{'B'}}}}};
    ASSERT_EQ(got_err.get(), want);

    cbdc::test::block want_block;
    want_block.m_height = 1;
    want_block.m_transactions.push_back(
        cbdc::test::simple_tx({'a'}, {{'B'}, {'c'}}, {{{'d'}, {'e'}, {'f'}}}));
    expect_block(want_block);
}
