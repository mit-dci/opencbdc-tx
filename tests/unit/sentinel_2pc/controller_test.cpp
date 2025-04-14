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
        constexpr auto sentinel_private_key
            = "000000000000000100000000000000000000000000000000000000000000000"
              "0";
        constexpr auto sentinel_public_key
            = "eaa649f21f51bdbae7be4ae34ce6e5217a58fdce7f47f9aa7f3b58fa2120e2b"
              "3";
        m_opts.m_sentinel_private_keys[0]
            = cbdc::hash_from_hex(sentinel_private_key);
        m_opts.m_sentinel_public_keys.insert(
            cbdc::hash_from_hex(sentinel_public_key));

        m_opts.m_coordinator_endpoints.resize(1);
        m_opts.m_coordinator_endpoints[0].push_back(coordinator_ep);

        // The locking shard endpoint defined below may not be used in tests,
        // but it must be defined for the options struct to be valid.  Without
        // it, the check_options function couldn't be used to validate the
        // other options used in this test.
        static constexpr unsigned short m_locking_shard_port = 42001;
        const auto locking_shard_endpoint
            = std::make_pair(cbdc::network::localhost, m_locking_shard_port);
        m_opts.m_locking_shard_endpoints.resize(1);
        m_opts.m_locking_shard_endpoints[0].push_back(locking_shard_endpoint);

        auto opt_chk_result = cbdc::config::check_options(m_opts);
        ASSERT_FALSE(opt_chk_result.has_value());

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
    std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)>
        m_secp{secp256k1_context_create(SECP256K1_CONTEXT_SIGN
                                        | SECP256K1_CONTEXT_VERIFY),
               &secp256k1_context_destroy};
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
        [&](std::optional<cbdc::sentinel::execute_response> resp) {
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

TEST_F(sentinel_2pc_test, tx_validation_test) {
    ASSERT_TRUE(m_ctl->init());
    auto ctx = cbdc::transaction::compact_tx(m_valid_tx);
    auto res
        = m_ctl->validate_transaction(m_valid_tx, [&](auto validation_res) {
              ASSERT_TRUE(validation_res.has_value());
              ASSERT_TRUE(ctx.verify(m_secp.get(), validation_res.value()));
          });
    ASSERT_TRUE(res);
    // ensures the validation callback has completed before we go out-of-scope
    m_ctl->stop();
}

TEST_F(sentinel_2pc_test, bad_coordinator_endpoint) {
    // Replace the valid coordinator endpoint defined in the fixture
    // with an invalid endpoint.
    m_opts.m_coordinator_endpoints.clear();
    const auto bad_coordinator_ep
        = std::make_pair("abcdefg", m_coordinator_port);
    m_opts.m_coordinator_endpoints.resize(1);
    m_opts.m_coordinator_endpoints[0].push_back(bad_coordinator_ep);

    // Initialize a new controller with the invalid coordinator endpoint.
    auto ctl = std::make_unique<cbdc::sentinel_2pc::controller>(0,
                                                                m_opts,
                                                                m_logger);

    // Check that the controller with the invalid coordinator endpoint
    // still initializes correctly.
    ASSERT_TRUE(ctl->init());
}

TEST_F(sentinel_2pc_test, bad_sentinel_client_endpoint) {
    // Test that a sentinel client fails to initialize
    // when given a bad endpoint.
    constexpr auto bad_endpoint = std::make_pair("abcdefg", m_sentinel_port);
    const std::vector<cbdc::network::endpoint_t> bad_endpoints{bad_endpoint};
    auto client = cbdc::sentinel::rpc::client(bad_endpoints, m_logger);
    ASSERT_FALSE(client.init());

    // Test that the controller initializes even when given a bad endpoint
    // for a sentinel client.
    m_opts.m_sentinel_endpoints.emplace_back(bad_endpoint);
    auto ctl = std::make_unique<cbdc::sentinel_2pc::controller>(0,
                                                                m_opts,
                                                                m_logger);
    ASSERT_TRUE(ctl->init());
}

TEST_F(sentinel_2pc_test, bad_rpc_server_endpoint) {
    // The sentinel endpoint defined below (which corresponds to sentinel_id
    // also defined below) is used by the sentinel_2pc controller to initialize
    // an rpc server.  Replacing the valid endpoint defined in the fixture with
    // an invalid endpoint should cause the rpc server to fail to initialize.
    m_opts.m_sentinel_endpoints.clear();
    constexpr auto bad_endpoint = std::make_pair("abcdefg", m_sentinel_port);
    m_opts.m_sentinel_endpoints.resize(1);
    m_opts.m_sentinel_endpoints.emplace_back(bad_endpoint);

    // Initialize a new controller with the invalid endpoint for the server.
    constexpr uint32_t sentinel_id = 0;
    const auto ctl
        = std::make_unique<cbdc::sentinel_2pc::controller>(sentinel_id,
                                                           m_opts,
                                                           m_logger);

    // Check that the controller with the invalid endpoint fails to initialize.
    ASSERT_FALSE(ctl->init());
}

TEST_F(sentinel_2pc_test, out_of_range_sentinel_id) {
    // Test that controller initialization fails when the sentinel ID is
    // too large for the number of sentinels.  Here, since there's only
    // 1 sentinel, the only allowable sentinel ID is 0.  However, it's
    // deliberately set to 1 to trigger failure.
    constexpr uint32_t bad_sentinel_id = 1;

    // Add private key for the bad sentinel ID to avoid triggering the error
    // "No private key specified".
    constexpr auto sentinel_private_key
        = "0000000000000001000000000000000000000000000000000000000000000001";
    m_opts.m_sentinel_private_keys[bad_sentinel_id]
        = cbdc::hash_from_hex(sentinel_private_key);

    auto ctl
        = std::make_unique<cbdc::sentinel_2pc::controller>(bad_sentinel_id,
                                                           m_opts,
                                                           m_logger);
    ASSERT_FALSE(ctl->init());
}

TEST_F(sentinel_2pc_test, no_sentinel_endpoints) {
    m_opts.m_sentinel_endpoints.clear();
    auto ctl = std::make_unique<cbdc::sentinel_2pc::controller>(0,
                                                                m_opts,
                                                                m_logger);

    // Check that the controller fails to initialize if no sentinel endpoints
    // are defined.
    ASSERT_FALSE(ctl->init());
}
