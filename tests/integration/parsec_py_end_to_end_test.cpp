// Copyright (c) 2023 MIT Digital Currency Initiative,
//
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "parsec/agent/client.hpp"
#include "parsec/agent/impl.hpp"
#include "parsec/agent/runners/py/impl.hpp"
#include "parsec/agent/runners/py/py_server.hpp"
#include "parsec/agent/runners/py/pybuffer.hpp"
#include "parsec/agent/runners/py/pyutil.hpp"
#include "parsec/broker/impl.hpp"
#include "parsec/broker/interface.hpp"
#include "parsec/directory/impl.hpp"
#include "parsec/runtime_locking_shard/impl.hpp"
#include "parsec/ticket_machine/impl.hpp"

#include <cstring>
#include <future>
#include <gtest/gtest.h>

namespace pythoncontracts {
    std::string pay_key = "pay_contract";
    std::string interest_key = "accrueInterest";
}

class parsec_py_end_to_end_test : public ::testing::Test {
  protected:
    void SetUp() override {
        const auto server_endpoint
            = cbdc::network::endpoint_t{"localhost", 8889};

        init_server_and_client(server_endpoint);
        init_accounts();
    }

    void init_accounts(std::string bal0 = "100", std::string bal1 = "400");

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

    std::string m_account0_pubkey;
    std::string m_account0_bal;

    std::string m_account1_pubkey;
    std::string m_account1_bal;
};

void parsec_py_end_to_end_test::init_server_and_client(
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
    m_server = std::make_unique<cbdc::parsec::agent::rpc::py_server>(
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

void parsec_py_end_to_end_test::init_accounts(std::string bal0,
                                              std::string bal1) {
    m_account0_bal = std::move(bal0);
    m_account1_bal = std::move(bal1);

    m_account0_pubkey = "Alice";
    m_account1_pubkey = "Bob";

    std::promise<void> complete;

    const auto check_ok_cb = [&complete](bool ok) {
        ASSERT_TRUE(ok);
        complete.set_value();
    };

    auto keys = std::vector<std::string>(0);
    auto values = std::vector<std::string>(0);

    keys.emplace_back("Alice");
    keys.emplace_back("Bob");
    keys.emplace_back(pythoncontracts::pay_key);
    keys.emplace_back(pythoncontracts::interest_key);
    keys.emplace_back("Interest Rate");
    keys.emplace_back("pay2");

    values.emplace_back("100");
    values.emplace_back("400");
    auto pay_contract = cbdc::parsec::pyutils::formContract(
        "../scripts/paycontract.py",
        "../scripts/pythonContractConverter.py",
        "pay");
    values.emplace_back(pay_contract.c_str());
    auto interest_contract = cbdc::parsec::pyutils::formContract(
        "../scripts/paycontract.py",
        "../scripts/pythonContractConverter.py",
        "accrueInterest");
    values.emplace_back(interest_contract.c_str());
    values.emplace_back("0.05");
    auto pay2Contract = cbdc::parsec::pyutils::formContract(
        "../scripts/paycontract.py",
        "../scripts/pythonContractConverter.py",
        "pay2");
    values.emplace_back(pay2Contract.c_str());

    for(size_t i = 0; i < keys.size(); i++) {
        auto key = cbdc::buffer();
        auto value = cbdc::buffer();

        key.append(keys[i].c_str(), keys[i].length() + 1);
        value.append(values[i].c_str(), values[i].length() + 1);

        cbdc::parsec::put_row(m_broker, key, value, check_ok_cb);

        auto fut = complete.get_future();
        fut.wait();
        complete = std::promise<void>();
        m_log->trace("DONE", i);
    }
}

TEST_F(parsec_py_end_to_end_test, init_accounts) {
    init_accounts();

    auto key = cbdc::buffer();
    key.append("Alice", 6);
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
    key.append("Bob", 4);
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
    EXPECT_STREQ(return_value.c_str(), "400");

    m_log->trace("Complete");
}

TEST_F(parsec_py_end_to_end_test, run_contract) {
    init_accounts();

    auto params = cbdc::parsec::pybuffer::pyBuffer();
    params.appendNumeric<int>(10);
    params.endSection();

    params.appendString("Alice");
    params.appendString("Bob");
    params.endSection();

    params.appendString("Alice");
    params.appendString("Bob");
    params.endSection();

    auto pay_contract_key = cbdc::buffer();
    pay_contract_key.append(pythoncontracts::pay_key.c_str(),
                            pythoncontracts::pay_key.size() + 1);
    auto r = m_agents[0]->exec(
        pay_contract_key,
        params,
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
        m_log->error("exec error");
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto key = cbdc::buffer();
    key.append("Alice", 6);
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
    EXPECT_STREQ(return_value.c_str(), "90");

    key = cbdc::buffer();
    key.append("Bob", 4);
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
    EXPECT_STREQ(return_value.c_str(), "410");
}

TEST_F(parsec_py_end_to_end_test, payTwoEntities) {
    init_accounts();

    auto params = cbdc::parsec::pybuffer::pyBuffer();
    params.appendNumeric<int>(10);
    params.appendNumeric<int>(20);
    params.endSection();

    params.appendString("Alice");
    params.appendString("Bob");
    params.appendString("Charlie");
    params.endSection();

    params.appendString("Alice");
    params.appendString("Bob");
    params.appendString("Charlie");
    params.endSection();

    auto pay_contract_key = cbdc::buffer();
    pay_contract_key.append("pay2", 5);
    auto r = m_agents[0]->exec(
        pay_contract_key,
        params,
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
        m_log->error("exec error");
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto key = cbdc::buffer();
    key.append("Alice", 6);
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
    EXPECT_STREQ(return_value.c_str(), "70");

    key = cbdc::buffer();
    key.append("Bob", 4);
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
    EXPECT_STREQ(return_value.c_str(), "410");

    key = cbdc::buffer();
    key.append("Charlie", 8);
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
    EXPECT_STREQ(return_value.c_str(), "20");
}

TEST_F(parsec_py_end_to_end_test, instantiate_user) {
    init_accounts();

    auto params = cbdc::parsec::pybuffer::pyBuffer();
    params.appendNumeric<int>(10);
    params.endSection();

    params.appendString("Alice");
    params.appendString("Charlie");
    params.endSection();

    params.appendString("Alice");
    params.appendString("Charlie");
    params.endSection();

    auto pay_contract_key = cbdc::buffer();
    pay_contract_key.append(pythoncontracts::pay_key.c_str(),
                            pythoncontracts::pay_key.size() + 1);
    auto r = m_agents[0]->exec(
        pay_contract_key,
        params,
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
        m_log->error("exec error");
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto key = cbdc::parsec::pybuffer::pyBuffer();
    key.appendString("Alice");
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
    EXPECT_STREQ(return_value.c_str(), "90");

    key = cbdc::parsec::pybuffer::pyBuffer();
    key.appendString("Charlie");
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
    EXPECT_STREQ(return_value.c_str(), "10");
}

TEST_F(parsec_py_end_to_end_test, accrue_interest) {
    init_accounts();
    auto params = cbdc::parsec::pybuffer::pyBuffer();
    params.endSection();
    params.appendString("Interest Rate");
    params.appendString("Alice");
    params.endSection();

    params.appendString("Alice");
    params.endSection();
    auto pay_contract_key = cbdc::buffer();
    pay_contract_key.append(pythoncontracts::interest_key.c_str(),
                            pythoncontracts::interest_key.size() + 1);

    auto r = m_agents[0]->exec(
        pay_contract_key,
        params,
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
        m_log->error("exec error");
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto key = cbdc::buffer();
    key.append("Alice", 6);
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
    EXPECT_STREQ(return_value.c_str(), "105");
}

// Try running an invalid contract, make sure that the passed in keys are
// unchanged
TEST_F(parsec_py_end_to_end_test, invalid_contract) {
    init_accounts();

    auto key = cbdc::buffer();
    key.append("Alice", 6);
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
    auto* aliceInitial = strdup(return_value.c_str());

    key = cbdc::buffer();
    key.append("Bob", 4);
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
    auto* bobInitial = strdup(return_value.c_str());

    auto params = cbdc::parsec::pybuffer::pyBuffer();
    params.appendNumeric<int>(10);
    params.endSection();

    params.appendString("Alice");
    params.appendString("Bob");
    params.endSection();

    params.appendString("Alice");
    params.appendString("Bob");
    params.endSection();

    auto pay_contract_key = cbdc::buffer();

    // Attempt to run a non-existent contract
    pay_contract_key.append("Not a key", 10);
    auto r = m_agents[0]->exec(
        pay_contract_key,
        params,
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
    ASSERT_TRUE(r);
    if(!r) {
        m_log->error("exec error");
    }

    key = cbdc::buffer();
    key.append("Alice", 6);
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
    EXPECT_STREQ(return_value.c_str(), aliceInitial);

    key = cbdc::buffer();
    key.append("Bob", 4);
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
    EXPECT_STREQ(return_value.c_str(), bobInitial);

    free(aliceInitial);
    free(bobInitial);
}

// Try sending an invalid payment, make sure that the passed in keys are
// unchanged
TEST_F(parsec_py_end_to_end_test, invalid_payment) {
    init_accounts("0", "400");

    auto key = cbdc::buffer();
    key.append("Alice", 6);
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
    auto* aliceInitial = strdup(return_value.c_str());

    key = cbdc::buffer();
    key.append("Bob", 4);
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
    auto* bobInitial = strdup(return_value.c_str());

    auto params = cbdc::parsec::pybuffer::pyBuffer();
    params.appendNumeric<int>(10);
    params.endSection();

    params.appendString("Alice");
    params.appendString("Bob");
    params.endSection();

    params.appendString("Alice");
    params.appendString("Bob");
    params.endSection();

    auto pay_contract_key = cbdc::buffer();
    pay_contract_key.append(pythoncontracts::pay_key.c_str(),
                            pythoncontracts::pay_key.size() + 1);

    auto r = m_agents[0]->exec(
        pay_contract_key,
        params,
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
    ASSERT_TRUE(r);
    if(!r) {
        m_log->error("exec error");
    }

    key = cbdc::buffer();
    key.append("Alice", 6);
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
    EXPECT_STREQ(return_value.c_str(), aliceInitial);

    key = cbdc::buffer();
    key.append("Bob", 4);
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
    EXPECT_STREQ(return_value.c_str(), bobInitial);

    free(aliceInitial);
    free(bobInitial);
}
