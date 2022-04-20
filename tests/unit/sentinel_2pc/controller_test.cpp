// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/sentinel/client.hpp"
#include "uhs/transaction/wallet.hpp"
#include "uhs/twophase/coordinator/controller.hpp"
#include "uhs/twophase/coordinator/format.hpp"
#include "uhs/twophase/sentinel_2pc/controller.hpp"
#include "util/serialization/util.hpp"
#include "util/serialization/util2.hpp"

#include <gtest/gtest.h>

class sentinel_2pc_test : public ::testing::Test {
  protected:
    void SetUp() override {
        m_dummy_coordinator_net = std::make_unique<
            decltype(m_dummy_coordinator_net)::element_type>();

        m_opts.m_twophase_mode = true;
        const auto sentinel_ep
            = std::make_pair(cbdc::network::localhost, m_sentinel_port);
        const auto coordinator_ep
            = std::make_pair(cbdc::network::localhost, m_coordinator_port);
        m_opts.m_sentinel_endpoints.push_back(sentinel_ep);
        m_opts.m_coordinator_endpoints.resize(1);
        m_opts.m_coordinator_endpoints[0].push_back(coordinator_ep);

        m_dummy_coordinator_thread = m_dummy_coordinator_net->start_server(
            coordinator_ep,
            [&](cbdc::network::message_t&& pkt)
                -> std::optional<cbdc::buffer> {
                auto req = cbdc::from_buffer<
                    cbdc::rpc::request<cbdc::coordinator::rpc::request>>(
                    *pkt.m_pkt);
                EXPECT_TRUE(req.has_value());
                std::this_thread::sleep_for(m_processing_delay);
                return cbdc::make_buffer(
                    cbdc::rpc::response<cbdc::coordinator::rpc::response>{
                        req->m_header,
                        true});
            });

        cbdc::transaction::wallet wallet1;
        cbdc::transaction::wallet wallet2;

        auto mint_tx1 = wallet1.mint_new_coins(3, 100);
        wallet1.confirm_transaction(mint_tx1);
        auto mint_tx2 = wallet2.mint_new_coins(1, 100);
        wallet2.confirm_transaction(mint_tx2);
        m_logger = std::make_shared<cbdc::logging::log>(
            cbdc::logging::log_level::debug);
        m_ctl = std::make_unique<cbdc::sentinel_2pc::controller>(0,
                                                                 m_opts,
                                                                 m_logger);

        m_valid_tx = wallet1.send_to(20, wallet2.generate_key(), true).value();
    }

    void TearDown() override {
        m_dummy_coordinator_net->close();

        if(m_dummy_coordinator_thread.has_value()) {
            m_dummy_coordinator_thread.value().join();
        }
    }

    static constexpr unsigned short m_coordinator_port = 32001;
    static constexpr unsigned short m_sentinel_port = 32002;
    static constexpr auto m_processing_delay = std::chrono::milliseconds(100);
    std::unique_ptr<cbdc::network::connection_manager> m_dummy_coordinator_net;
    std::optional<std::thread> m_dummy_coordinator_thread;
    cbdc::config::options m_opts{};
    std::unique_ptr<cbdc::sentinel_2pc::controller> m_ctl;
    cbdc::transaction::full_tx m_valid_tx{};
    std::shared_ptr<cbdc::logging::log> m_logger;
};

TEST_F(sentinel_2pc_test, test_init) {
    ASSERT_TRUE(m_ctl->init());
}

TEST_F(sentinel_2pc_test, test_init_sentinel_port_not_available) {
    auto dummy_conflicting_sentinel_net
        = std::make_unique<cbdc::network::connection_manager>();
    ASSERT_TRUE(
        dummy_conflicting_sentinel_net->listen(cbdc::network::localhost,
                                               m_sentinel_port));
    ASSERT_FALSE(m_ctl->init());
}

TEST_F(sentinel_2pc_test, digest_invalid_transaction_direct) {
    m_ctl->init();
    m_valid_tx.m_inputs.clear();
    auto done = std::promise<void>();
    auto done_fut = done.get_future();
    auto res = m_ctl->execute_transaction(
        m_valid_tx,
        [&](std::optional<cbdc::sentinel::response> resp) {
            ASSERT_TRUE(resp.has_value());
            ASSERT_TRUE(resp.value().m_tx_error.has_value());
            ASSERT_EQ(resp.value().m_tx_status,
                      cbdc::sentinel::tx_status::static_invalid);
            done.set_value();
        });
    ASSERT_TRUE(res);
    auto r = done_fut.wait_for(std::chrono::milliseconds(500));
    ASSERT_EQ(r, std::future_status::ready);
}

TEST_F(sentinel_2pc_test, digest_invalid_transaction_network) {
    m_ctl->init();
    m_valid_tx.m_inputs.clear();

    auto client = cbdc::sentinel::rpc::client(
        {{cbdc::network::localhost, m_sentinel_port}},
        m_logger);
    ASSERT_TRUE(client.init());
    auto resp = client.execute_transaction(m_valid_tx);
    ASSERT_TRUE(resp.has_value());
    ASSERT_TRUE(resp.value().m_tx_error.has_value());
    ASSERT_EQ(resp.value().m_tx_status,
              cbdc::sentinel::tx_status::static_invalid);
}

TEST_F(sentinel_2pc_test, digest_valid_transaction_direct) {
    m_ctl->init();
    auto res = m_ctl->execute_transaction(m_valid_tx, [](auto /* param */) {});
    ASSERT_TRUE(res);
}

TEST_F(sentinel_2pc_test, digest_valid_transaction_network) {
    m_ctl->init();

    auto client = cbdc::sentinel::rpc::client(
        {{cbdc::network::localhost, m_sentinel_port}},
        m_logger);
    ASSERT_TRUE(client.init());
    auto resp = client.execute_transaction(m_valid_tx);
    ASSERT_TRUE(resp.has_value());
    ASSERT_FALSE(resp.value().m_tx_error.has_value());
    ASSERT_EQ(resp.value().m_tx_status, cbdc::sentinel::tx_status::confirmed);
}
