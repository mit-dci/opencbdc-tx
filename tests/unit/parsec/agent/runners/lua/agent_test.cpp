// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "../../../util.hpp"
#include "parsec/agent/impl.hpp"
#include "parsec/agent/runners/lua/impl.hpp"
#include "parsec/broker/impl.hpp"
#include "parsec/directory/impl.hpp"
#include "parsec/runtime_locking_shard/impl.hpp"
#include "parsec/ticket_machine/impl.hpp"

#include <gtest/gtest.h>
#include <thread>

class agent_test : public ::testing::Test {
  protected:
    void SetUp() override {
        m_deploy_contract_key.append("deploy", 6);
        m_deploy_contract
            = cbdc::buffer::from_hex("1b4c7561540019930d0a1a0a0408087856000000"
                                     "00000000000000002877400"
                                     "1808187010004968b0000028e000103030102008"
                                     "0010000c40003030f000102"
                                     "0f0000018b0000068e0001070b010000c4000202"
                                     "0f000501930000005200000"
                                     "00f0008018b0000080b0100008b0100019000020"
                                     "38b000008c8000200c70001"
                                     "008904846b6579048566756e630487737472696e"
                                     "670487756e7061636b04837"
                                     "373048276048a636f726f7574696e65048679696"
                                     "56c64048274810000008080"
                                     "808080")
                  .value();
        cbdc::test::add_to_shard(m_broker,
                                 m_deploy_contract_key,
                                 m_deploy_contract);
    }

    std::shared_ptr<cbdc::logging::log> m_log{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::trace)};
    cbdc::parsec::config m_cfg{};
    std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface> m_shard0{
        std::make_shared<cbdc::parsec::runtime_locking_shard::impl>(m_log)};
    std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface> m_shard1{
        std::make_shared<cbdc::parsec::runtime_locking_shard::impl>(m_log)};
    std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface> m_shard2{
        std::make_shared<cbdc::parsec::runtime_locking_shard::impl>(m_log)};
    std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface> m_shard3{
        std::make_shared<cbdc::parsec::runtime_locking_shard::impl>(m_log)};
    std::shared_ptr<cbdc::parsec::ticket_machine::interface> m_ticketer{
        std::make_shared<cbdc::parsec::ticket_machine::impl>(m_log, 1)};
    std::shared_ptr<cbdc::parsec::directory::interface> m_directory{
        std::make_shared<cbdc::parsec::directory::impl>(4)};
    std::shared_ptr<cbdc::parsec::broker::interface> m_broker{
        std::make_shared<cbdc::parsec::broker::impl>(
            0,
            std::vector<std::shared_ptr<
                cbdc::parsec::runtime_locking_shard::interface>>(
                {m_shard0, m_shard1, m_shard2, m_shard3}),
            m_ticketer,
            m_directory,
            m_log)};

    cbdc::buffer m_deploy_contract_key;
    cbdc::buffer m_deploy_contract;
};

TEST_F(agent_test, deploy_test) {
    auto params = cbdc::buffer();
    auto contract_name = cbdc::buffer();
    static constexpr uint64_t key_len = 8;
    contract_name.append("contract", key_len);
    params.append(&key_len, sizeof(key_len));
    params.append("contract", key_len);
    uint64_t fun_len = m_deploy_contract.size();
    params.append(&fun_len, sizeof(fun_len));
    params.append(m_deploy_contract.data(), m_deploy_contract.size());

    auto agent = std::make_shared<cbdc::parsec::agent::impl>(
        m_log,
        m_cfg,
        &cbdc::parsec::agent::runner::factory<
            cbdc::parsec::agent::runner::lua_runner>::create,
        m_broker,
        m_deploy_contract_key,
        params,
        [&](cbdc::parsec::agent::interface::exec_return_type res) {
            ASSERT_TRUE(
                std::holds_alternative<cbdc::parsec::agent::return_type>(res));
            auto ret = cbdc::parsec::agent::return_type(
                {{contract_name, m_deploy_contract}});
            ASSERT_EQ(ret, std::get<cbdc::parsec::agent::return_type>(res));
        },
        cbdc::parsec::agent::runner::lua_runner::initial_lock_type,
        false,
        nullptr,
        nullptr);
    ASSERT_TRUE(agent->exec());
}

TEST_F(agent_test, rollback_test) {
    auto params = cbdc::buffer();

    auto callback_called = false;
    auto agent = std::make_shared<cbdc::parsec::agent::impl>(
        m_log,
        m_cfg,
        &cbdc::parsec::agent::runner::factory<
            cbdc::parsec::agent::runner::lua_runner>::create,
        m_broker,
        m_deploy_contract_key,
        params,
        [&](const cbdc::parsec::agent::interface::exec_return_type& res) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::agent::interface::error_code>(res));
            ASSERT_EQ(
                std::get<cbdc::parsec::agent::interface::error_code>(res),
                cbdc::parsec::agent::interface::error_code::
                    function_execution);
            ASSERT_FALSE(callback_called);
            callback_called = true;
        },
        cbdc::parsec::agent::runner::lua_runner::initial_lock_type,
        false,
        nullptr,
        nullptr);
    ASSERT_TRUE(agent->exec());
    ASSERT_TRUE(agent->exec());
}

TEST_F(agent_test, wound_deploy_test) {
    auto params = cbdc::buffer();
    auto contract_name = cbdc::buffer();
    static constexpr uint64_t key_len = 8;
    contract_name.append("contract", key_len);
    params.append(&key_len, sizeof(key_len));
    params.append("contract", key_len);
    uint64_t fun_len = m_deploy_contract.size();
    params.append(&fun_len, sizeof(fun_len));
    params.append(m_deploy_contract.data(), m_deploy_contract.size());

    auto broker1 = std::make_shared<cbdc::parsec::broker::impl>(
        1,
        std::vector<
            std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface>>(
            {m_shard0, m_shard1, m_shard2, m_shard3}),
        m_ticketer,
        m_directory,
        m_log);

    auto agent0_complete = false;
    auto agent1_complete = false;
    auto agent0 = std::make_shared<cbdc::parsec::agent::impl>(
        m_log,
        m_cfg,
        &cbdc::parsec::agent::runner::factory<
            cbdc::parsec::agent::runner::lua_runner>::create,
        m_broker,
        m_deploy_contract_key,
        params,
        [&](cbdc::parsec::agent::interface::exec_return_type res) {
            ASSERT_TRUE(
                std::holds_alternative<cbdc::parsec::agent::return_type>(res));
            auto ret = cbdc::parsec::agent::return_type(
                {{contract_name, m_deploy_contract}});
            ASSERT_EQ(ret, std::get<cbdc::parsec::agent::return_type>(res));
            agent0_complete = true;
        },
        cbdc::parsec::agent::runner::lua_runner::initial_lock_type,
        false,
        nullptr,
        nullptr);
    auto agent1 = std::make_shared<cbdc::parsec::agent::impl>(
        m_log,
        m_cfg,
        &cbdc::parsec::agent::runner::factory<
            cbdc::parsec::agent::runner::lua_runner>::create,
        broker1,
        m_deploy_contract_key,
        params,
        [&](cbdc::parsec::agent::interface::exec_return_type res) {
            ASSERT_TRUE(
                std::holds_alternative<cbdc::parsec::agent::return_type>(res));
            auto ret = cbdc::parsec::agent::return_type(
                {{contract_name, m_deploy_contract}});
            ASSERT_EQ(ret, std::get<cbdc::parsec::agent::return_type>(res));
            agent1_complete = true;
        },
        cbdc::parsec::agent::runner::lua_runner::initial_lock_type,
        false,
        nullptr,
        nullptr);
    std::thread t0([&]() {
        ASSERT_TRUE(agent0->exec());
    });
    std::thread t1([&]() {
        ASSERT_TRUE(agent1->exec());
    });
    t0.join();
    t1.join();
    ASSERT_TRUE(agent0_complete);
    ASSERT_TRUE(agent1_complete);
}
