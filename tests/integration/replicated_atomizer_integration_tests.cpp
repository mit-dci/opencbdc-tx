// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mock_system.hpp"
#include "uhs/atomizer/atomizer/controller.hpp"
#include "uhs/atomizer/atomizer/format.hpp"
#include "util.hpp"
#include "util/common/hashmap.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <vector>

class replicated_atomizer_integration_tests : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::test::load_config(m_atomizer_cfg_path, m_opts);
        m_opts.m_attestation_threshold = 0;
        m_atomizer_count = m_opts.m_atomizer_endpoints.size();
        for(auto i = 0ULL; i < m_atomizer_count; ++i) {
            m_ctls.push_back(
                std::make_unique<cbdc::atomizer::controller>(i,
                                                             m_opts,
                                                             m_logger));
        }

        auto ignore_set = std::unordered_set<cbdc::test::mock_system_module>(
            {cbdc::test::mock_system_module::atomizer});
        m_sys = std::make_unique<cbdc::test::mock_system>(ignore_set, m_opts);
        m_sys->init();

        auto init_threads = std::vector<std::thread>(m_atomizer_count);
        for(auto i = 1ULL; i < m_atomizer_count; ++i) {
            std::thread t([&, idx = i]() {
                ASSERT_TRUE(m_ctls[idx]->init());
            });

            init_threads[i] = std::move(t);
        }

        // NB: Must start leader (always node 0 to start) after all followers
        std::thread t([&]() {
            ASSERT_TRUE(m_ctls[0]->init());
        });

        init_threads[0] = std::move(t);
        for(auto& thr : init_threads) {
            thr.join();
        }

        m_cluster.cluster_connect(m_opts.m_atomizer_endpoints, false);
        m_block_client_thread
            = m_cluster.start_handler([&](cbdc::network::message_t&& pkt)
                                          -> std::optional<cbdc::buffer> {
                  auto res = cbdc::from_buffer<cbdc::test::block>(*pkt.m_pkt);
                  if(res.has_value()) {
                      std::unique_lock lk{m_bm};
                      for(const auto& tx : res->m_transactions) {
                          auto [it, success] = m_received_txs.insert(
                              cbdc::test::compact_transaction(tx));

                          if(!success) {
                              ADD_FAILURE()
                                  << "Received a duplicate transaction";
                          }
                      }
                  }
                  m_bcv.notify_all();
                  return std::nullopt;
              });
    }

    void TearDown() override {
        m_cluster.close();

        if(m_block_client_thread.joinable()) {
            m_block_client_thread.join();
        }

        std::filesystem::remove_all("archiver0_db");
        for(auto i = 0ULL; i < m_atomizer_count; ++i) {
            std::filesystem::remove_all("atomizer_raft_log_"
                                        + std::to_string(i));
            std::filesystem::remove_all("atomizer_raft_config_"
                                        + std::to_string(i) + ".dat");
            std::filesystem::remove_all("atomizer_raft_state_"
                                        + std::to_string(i) + ".dat");
            std::filesystem::remove_all("atomizer_snps_" + std::to_string(i));
        }
    }

    void expect_tx(const cbdc::test::compact_transaction& tx,
                   const std::chrono::seconds& timeout
                   = std::chrono::seconds(5)) {
        std::unique_lock lk{m_bm};
        bool match{true};
        auto res = m_bcv.wait_for(lk, timeout, [&] {
            auto it = m_received_txs.find(tx);
            match = it != m_received_txs.end();
            return match;
        });
        if(!match) {
            ADD_FAILURE() << "Did not receive expected transaction";
        } else if(!res) {
            ADD_FAILURE() << "No transactions received.";
        }
    }

    static constexpr auto m_atomizer_cfg_path = "replicated_atomizer.cfg";

    cbdc::config::options m_opts{};
    size_t m_atomizer_count{0};

    std::unique_ptr<cbdc::test::mock_system> m_sys;

    std::shared_ptr<cbdc::logging::log> m_logger{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::trace)};

    std::vector<std::unique_ptr<cbdc::atomizer::controller>> m_ctls;
    cbdc::network::connection_manager m_cluster;

    std::thread m_block_client_thread;
    std::unordered_set<cbdc::test::compact_transaction,
                       cbdc::test::compact_transaction_hasher>
        m_received_txs;
    std::mutex m_bm;
    std::condition_variable m_bcv;
};

TEST_F(replicated_atomizer_integration_tests,
       can_send_message_from_clustered_atomizer) {
    auto tx = cbdc::test::simple_tx({'a'}, {}, {{{'c'}}});
    ASSERT_TRUE(m_cluster.send_to_one(cbdc::atomizer::request{
        cbdc::atomizer::tx_notify_request{tx, {}, 0}}));

    expect_tx(tx, std::chrono::seconds(5));
}
