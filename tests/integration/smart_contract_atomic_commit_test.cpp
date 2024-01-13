// Copyright (c) 2024 MIT Digital Currency Initiative,
//
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "parsec/agent/client.hpp"
#include "parsec/agent/impl.hpp"
#include "parsec/agent/runners/lua/impl.hpp"
#include "parsec/agent/runners/lua/server.hpp"
#include "parsec/broker/impl.hpp"
#include "parsec/broker/interface.hpp"
#include "parsec/directory/impl.hpp"
#include "parsec/runtime_locking_shard/impl.hpp"
#include "parsec/ticket_machine/impl.hpp"

#include <future>
#include <gtest/gtest.h>
#include <lua.hpp>

class parsec_smart_contract_updates_test : public ::testing::Test {
  protected:
    void SetUp() override {
        const auto server_endpoint
            = cbdc::network::endpoint_t{"localhost", 8889};

        init_server_and_client(server_endpoint);
        init_accounts();
    }

    void init_accounts();

    void
    init_server_and_client(const cbdc::network::endpoint_t& server_endpoint);

    std::shared_ptr<cbdc::logging::log> m_log{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::warn)};

    cbdc::parsec::config m_cfg{};

    std::shared_ptr<cbdc::parsec::broker::interface> m_broker;
    std::vector<
        std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface>>
        m_shards;
    std::shared_ptr<cbdc::parsec::ticket_machine::interface> m_ticket_machine;
    std::shared_ptr<cbdc::parsec::directory::interface> m_directory;

    std::unique_ptr<cbdc::parsec::agent::rpc::server_interface> m_server;
    std::vector<std::shared_ptr<cbdc::parsec::agent::rpc::client>> m_agents;
};

void parsec_smart_contract_updates_test::init_server_and_client(
    const cbdc::network::endpoint_t& server_endpoint) {
    m_ticket_machine
        = std::make_shared<cbdc::parsec::ticket_machine::impl>(m_log, 1);
    m_directory = std::make_shared<cbdc::parsec::directory::impl>(1);
    m_shards = std::vector<
        std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface>>(
        {std::make_shared<cbdc::parsec::runtime_locking_shard::impl>(m_log)});

    m_broker = std::make_shared<cbdc::parsec::broker::impl>(0,
                                                            m_shards,
                                                            m_ticket_machine,
                                                            m_directory,
                                                            m_log);

    m_server = std::unique_ptr<cbdc::parsec::agent::rpc::server_interface>();
    m_server = std::make_unique<cbdc::parsec::agent::rpc::server>(
        std::make_unique<
            cbdc::rpc::async_tcp_server<cbdc::parsec::agent::rpc::request,
                                        cbdc::parsec::agent::rpc::response>>(
            server_endpoint),
        m_broker,
        m_log,
        m_cfg);

    ASSERT_TRUE(m_server->init());
    bool running = true;
    m_agents
        = std::vector<std::shared_ptr<cbdc::parsec::agent::rpc::client>>();

    auto agent = std::make_shared<cbdc::parsec::agent::rpc::client>(
        std::vector<cbdc::network::endpoint_t>{server_endpoint});
    if(!agent->init()) {
        m_log->error("Error connecting to agent");
        running = false;
    } else {
        m_log->trace("Connected to agent");
    }
    m_agents.emplace_back(agent);

    ASSERT_TRUE(running);
}

void parsec_smart_contract_updates_test::init_accounts() {
    std::promise<void> complete;

    const auto check_ok_cb = [&complete](bool ok) {
        ASSERT_TRUE(ok);
        complete.set_value();
    };

    auto keys = std::vector<std::string>(0);
    auto values = std::vector<std::string>(0);

    keys.emplace_back("ticketed_key_1");
    keys.emplace_back("ticketed_key_2");
    keys.emplace_back("ticketed_key_3");
    keys.emplace_back("ticketed_key_4");
    keys.emplace_back("unticketed_key");

    values.emplace_back("1");
    values.emplace_back("2");
    values.emplace_back("3");
    values.emplace_back("4");
    values.emplace_back("4");

    const auto* contract_file
        = "../tests/integration/correct_state_update.lua";
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dofile(L, contract_file);
    lua_getglobal(L, "gen_bytecode");
    if(lua_pcall(L, 0, 2, 0) != 0) {
        m_log->error("Contract bytecode generation failed, with error:",
                     lua_tostring(L, -1));
        return;
    }
    auto valid_updates_contract = cbdc::buffer();
    valid_updates_contract
        = cbdc::buffer::from_hex(lua_tostring(L, -2)).value();
    auto valid_updates_key = cbdc::buffer();
    valid_updates_key.append("valid_updates", 14);

    m_log->trace("Inserting valid contract");
    auto init_error = std::atomic_bool{false};
    auto ret
        = cbdc::parsec::put_row(m_broker,
                                valid_updates_key,
                                valid_updates_contract,
                                [&](bool res) {
                                    if(!res) {
                                        init_error = true;
                                    } else {
                                        m_log->info("Inserted valid contract");
                                    }
                                });
    if(!ret || init_error) {
        m_log->error("Error adding valid contract");
        return;
    }

    const auto* hazard_contract_file
        = "../tests/integration/data_hazard_contract.lua";
    L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dofile(L, hazard_contract_file);
    lua_getglobal(L, "gen_bytecode");
    if(lua_pcall(L, 0, 2, 0) != 0) {
        m_log->error("Contract bytecode generation failed, with error:",
                     lua_tostring(L, -1));
        return;
    }
    auto invalid_updates_contract = cbdc::buffer();
    invalid_updates_contract
        = cbdc::buffer::from_hex(lua_tostring(L, -2)).value();
    auto invalid_updates_key = cbdc::buffer();
    invalid_updates_key.append("invalid_updates", 16);

    m_log->trace("Inserting invalid contract");
    init_error = false;
    ret = cbdc::parsec::put_row(m_broker,
                                invalid_updates_key,
                                invalid_updates_contract,
                                [&](bool res) {
                                    if(!res) {
                                        init_error = true;
                                    } else {
                                        m_log->info(
                                            "Inserted invalid contract");
                                    }
                                });
    if(!ret || init_error) {
        m_log->error("Error adding valid contract");
        return;
    }

    for(size_t i = 0; i < keys.size(); i++) {
        auto key = cbdc::buffer();
        auto value = cbdc::buffer();

        key.append(keys[i].c_str(), keys[i].length());
        value.append(values[i].c_str(), values[i].length() + 1);

        cbdc::parsec::put_row(m_broker, key, value, check_ok_cb);

        auto fut = complete.get_future();
        fut.wait();
        complete = std::promise<void>();
        m_log->trace("DONE", i);
    }
}

TEST_F(parsec_smart_contract_updates_test, valid_updates) {
    init_accounts();

    auto valid_key = cbdc::buffer();
    valid_key.append("valid_updates", 14);
    auto r = m_agents[0]->exec(
        valid_key,
        cbdc::buffer(),
        false,
        [&](const cbdc::parsec::agent::interface::exec_return_type& res) {
            auto success
                = std::holds_alternative<cbdc::parsec::agent::return_type>(
                    res);
            if(success) {
                m_log->info("Exec succeeded");
            } else {
                m_log->warn("Exec failed");
            }
        });
    if(!r) {
        m_log->error("Unexpected exec error");
    }

    // Avoid data race
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto key = cbdc::buffer();
    key.append("ticketed_key_1", 14);
    auto return_value = cbdc::buffer();
    auto complete = std::promise<void>();
    cbdc::parsec::get_row(
        m_broker,
        key,
        [&](cbdc::parsec::broker::interface::try_lock_return_type res) {
            if(std::holds_alternative<
                   cbdc::parsec::runtime_locking_shard::value_type>(res)) {
                auto found = std::get<
                    cbdc::parsec::runtime_locking_shard::value_type>(res);
                return_value = found;
                complete.set_value();
            } else {
                m_log->error("get row callback recieved error");
            }
        });
    auto fut_res = complete.get_future().wait_for(std::chrono::seconds(10));
    ASSERT_EQ(fut_res, std::future_status::ready);
    EXPECT_STREQ(return_value.c_str(), "100");

    key = cbdc::buffer();
    key.append("ticketed_key_2", 14);
    return_value = cbdc::buffer();
    complete = std::promise<void>();
    cbdc::parsec::get_row(
        m_broker,
        key,
        [&](cbdc::parsec::broker::interface::try_lock_return_type res) {
            if(std::holds_alternative<
                   cbdc::parsec::runtime_locking_shard::value_type>(res)) {
                auto found = std::get<
                    cbdc::parsec::runtime_locking_shard::value_type>(res);
                return_value = found;
                complete.set_value();
            } else {
                m_log->error("get row callback recieved error");
            }
        });
    fut_res = complete.get_future().wait_for(std::chrono::seconds(10));
    ASSERT_EQ(fut_res, std::future_status::ready);
    EXPECT_STREQ(return_value.c_str(), "200");

    key = cbdc::buffer();
    key.append("ticketed_key_3", 14);
    return_value = cbdc::buffer();
    complete = std::promise<void>();
    cbdc::parsec::get_row(
        m_broker,
        key,
        [&](cbdc::parsec::broker::interface::try_lock_return_type res) {
            if(std::holds_alternative<
                   cbdc::parsec::runtime_locking_shard::value_type>(res)) {
                auto found = std::get<
                    cbdc::parsec::runtime_locking_shard::value_type>(res);
                return_value = found;
                complete.set_value();
            } else {
                m_log->error("get row callback recieved error");
            }
        });
    fut_res = complete.get_future().wait_for(std::chrono::seconds(10));
    ASSERT_EQ(fut_res, std::future_status::ready);
    EXPECT_STREQ(return_value.c_str(), "250");

    key = cbdc::buffer();
    key.append("ticketed_key_4", 14);
    return_value = cbdc::buffer();
    complete = std::promise<void>();
    cbdc::parsec::get_row(
        m_broker,
        key,
        [&](cbdc::parsec::broker::interface::try_lock_return_type res) {
            if(std::holds_alternative<
                   cbdc::parsec::runtime_locking_shard::value_type>(res)) {
                auto found = std::get<
                    cbdc::parsec::runtime_locking_shard::value_type>(res);
                return_value = found;
                complete.set_value();
            } else {
                m_log->error("get row callback recieved error");
            }
        });
    fut_res = complete.get_future().wait_for(std::chrono::seconds(10));
    ASSERT_EQ(fut_res, std::future_status::ready);
    EXPECT_STREQ(return_value.c_str(), "255");

    m_log->trace("Complete");
}

TEST_F(parsec_smart_contract_updates_test, invalid_updates) {
    init_accounts();

    auto valid_key = cbdc::buffer();
    valid_key.append("invalid_updates", 16);
    auto r = m_agents[0]->exec(
        valid_key,
        cbdc::buffer(),
        false,
        [&](const cbdc::parsec::agent::interface::exec_return_type& res) {
            auto success
                = std::holds_alternative<cbdc::parsec::agent::return_type>(
                    res);
            if(success) {
                m_log->info("Exec succeeded");
            } else {
                m_log->warn("Exec failed");
            }
        });
    if(!r) {
        m_log->error("Unexpected exec error");
    }

    // Avoid data race
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto key = cbdc::buffer();
    key.append("ticketed_key_1", 14);
    auto return_value = cbdc::buffer();
    auto complete = std::promise<void>();
    cbdc::parsec::get_row(
        m_broker,
        key,
        [&](cbdc::parsec::broker::interface::try_lock_return_type res) {
            if(std::holds_alternative<
                   cbdc::parsec::runtime_locking_shard::value_type>(res)) {
                auto found = std::get<
                    cbdc::parsec::runtime_locking_shard::value_type>(res);
                return_value = found;
                complete.set_value();
            } else {
                m_log->error("get row callback recieved error");
            }
        });
    auto fut_res = complete.get_future().wait_for(std::chrono::seconds(10));
    ASSERT_EQ(fut_res, std::future_status::ready);
    EXPECT_STREQ(return_value.c_str(), "1");

    key = cbdc::buffer();
    key.append("ticketed_key_2", 14);
    return_value = cbdc::buffer();
    complete = std::promise<void>();
    cbdc::parsec::get_row(
        m_broker,
        key,
        [&](cbdc::parsec::broker::interface::try_lock_return_type res) {
            if(std::holds_alternative<
                   cbdc::parsec::runtime_locking_shard::value_type>(res)) {
                auto found = std::get<
                    cbdc::parsec::runtime_locking_shard::value_type>(res);
                return_value = found;
                complete.set_value();
            } else {
                m_log->error("get row callback recieved error");
            }
        });
    fut_res = complete.get_future().wait_for(std::chrono::seconds(10));
    ASSERT_EQ(fut_res, std::future_status::ready);
    EXPECT_STREQ(return_value.c_str(), "2");

    key = cbdc::buffer();
    key.append("ticketed_key_3", 14);
    return_value = cbdc::buffer();
    complete = std::promise<void>();
    cbdc::parsec::get_row(
        m_broker,
        key,
        [&](cbdc::parsec::broker::interface::try_lock_return_type res) {
            if(std::holds_alternative<
                   cbdc::parsec::runtime_locking_shard::value_type>(res)) {
                auto found = std::get<
                    cbdc::parsec::runtime_locking_shard::value_type>(res);
                return_value = found;
                complete.set_value();
            } else {
                m_log->error("get row callback recieved error");
            }
        });
    fut_res = complete.get_future().wait_for(std::chrono::seconds(10));
    ASSERT_EQ(fut_res, std::future_status::ready);
    EXPECT_STREQ(return_value.c_str(), "3");

    key = cbdc::buffer();
    key.append("unticketed_key", 14);
    return_value = cbdc::buffer();
    complete = std::promise<void>();
    cbdc::parsec::get_row(
        m_broker,
        key,
        [&](cbdc::parsec::broker::interface::try_lock_return_type res) {
            if(std::holds_alternative<
                   cbdc::parsec::runtime_locking_shard::value_type>(res)) {
                auto found = std::get<
                    cbdc::parsec::runtime_locking_shard::value_type>(res);
                return_value = found;
                complete.set_value();
            } else {
                m_log->error("get row callback recieved error");
            }
        });
    fut_res = complete.get_future().wait_for(std::chrono::seconds(10));
    ASSERT_EQ(fut_res, std::future_status::ready);
    EXPECT_STREQ(return_value.c_str(), "4");

    m_log->trace("Complete");
}
