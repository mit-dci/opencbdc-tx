// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/sha256.h"
#include "parsec/agent/client.hpp"
#include "parsec/agent/runners/py/pybuffer.hpp"
#include "parsec/agent/runners/py/pyutil.hpp"
#include "parsec/broker/impl.hpp"
#include "parsec/directory/impl.hpp"
#include "parsec/runtime_locking_shard/client.hpp"
#include "parsec/ticket_machine/client.hpp"
#include "parsec/util.hpp"
#include "wallet.hpp"

#include <Python.h>
#include <iostream>
#include <random>
#include <string>
#include <thread>

auto main(int argc, char** argv) -> int {
    // SETUP START
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);

    auto sha2_impl = SHA256AutoDetect();
    log->info("using sha2: ", sha2_impl);

    if(argc < 2) {
        log->error("Not enough arguments");
        return 1;
    }
    auto cfg = cbdc::parsec::read_config(argc, argv);
    if(!cfg.has_value()) {
        log->error("Error parsing options");
        return 1;
    }

    log->trace("Connecting to shards");
    auto shards = std::vector<
        std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface>>();
    for(const auto& shard_ep : cfg->m_shard_endpoints) {
        auto client = std::make_shared<
            cbdc::parsec::runtime_locking_shard::rpc::client>(
            std::vector<cbdc::network::endpoint_t>{shard_ep});
        if(!client->init()) {
            log->error("Error connecting to shard");
            return 1;
        }
        shards.emplace_back(client);
    }
    log->trace("Connected to shards");

    log->trace("Connecting to ticket machine");
    auto ticketer
        = std::make_shared<cbdc::parsec::ticket_machine::rpc::client>(
            std::vector<cbdc::network::endpoint_t>{
                cfg->m_ticket_machine_endpoints});
    if(!ticketer->init()) {
        log->error("Error connecting to ticket machine");
        return 1;
    }
    log->trace("Connected to ticket machine");

    auto directory
        = std::make_shared<cbdc::parsec::directory::impl>(shards.size());
    auto broker = std::make_shared<cbdc::parsec::broker::impl>(
        std::numeric_limits<size_t>::max(),
        shards,
        ticketer,
        directory,
        log);

    auto agents
        = std::vector<std::shared_ptr<cbdc::parsec::agent::rpc::client>>();
    for(auto& a : cfg->m_agent_endpoints) {
        auto agent = std::make_shared<cbdc::parsec::agent::rpc::client>(
            std::vector<cbdc::network::endpoint_t>{a});
        if(!agent->init()) {
            log->error("Error connecting to agent");
            return 1;
        }
        log->trace("Connected to agent");

        agents.emplace_back(agent);
    }
    // SETUP END

    // TICKET LOGGER STARTING
    std::atomic_bool running = true;
    std::thread ticket_state_logger([&broker, &running] {
        static constexpr auto log_timestep = 10;
        while(running) {
            broker->log_tickets();
            std::this_thread::sleep_for(std::chrono::seconds(log_timestep));
        }
    });
    ticket_state_logger.detach();
    // TICKET LOGGER STARTED

    // REGISTERING PAY CONTRACT
    auto pay_contract = cbdc::parsec::pybuffer::pyBuffer();
    auto contract_code = cbdc::parsec::pyutils::formContract(
        "scripts/paycontract.py",
        "scripts/pythonContractConverter.py",
        "pay");
    pay_contract.appendString(contract_code);

    auto init_error = std::atomic_bool{false};

    const std::string pay_key = "pay_contract";
    auto pay_contract_key = cbdc::buffer();
    pay_contract_key.append(pay_key.c_str(), pay_key.size());

    log->info("Inserting pay contract");

    auto prom_contract_placed = std::promise<void>();
    auto contract_emplaced = cbdc::parsec::put_row(
        broker,
        pay_contract_key,
        pay_contract,
        [&](bool res) {
            if(res) {
                log->info("Inserted pay contract", pay_contract.c_str());
                prom_contract_placed.set_value();
            } else {
                init_error = true;
                prom_contract_placed.set_value();
            }
        });
    if(!contract_emplaced) {
        init_error = true;
    }
    static constexpr auto future_timeout = 10;
    auto fut_contract_placed = prom_contract_placed.get_future().wait_for(
        std::chrono::seconds(future_timeout));
    if(init_error || fut_contract_placed != std::future_status::ready) {
        log->error("Error placing contract");
        return 2;
    }

    const std::vector<std::string> names
        = {"Alice", "Bob",    "Charlie",  "Diane",   "Edgar",   "Frank",
           "Greg",  "Henri",  "Isabelle", "Jessica", "Kathryn", "Laura",
           "Mike",  "Noelle", "Oscar",    "Patrick", "Quentin", "Rachel",
           "Sarah", "Tom",    "Ulysses",  "Victor",  "Walter",  "Xander",
           "Yana",  "Zach"};

    auto init_count = std::atomic<size_t>();

    std::vector<cbdc::parsec::pybench_wallet> wallets;

    /// \todo Read this in from config
    static constexpr auto n_wallets = 26;

    for(size_t i = 0; i < n_wallets; i++) {
        auto walletName
            = names[i % names.size()] + std::to_string(i / names.size());
        wallets.emplace_back(
            cbdc::parsec::pybench_wallet(log,
                                         broker,
                                         agents[i % agents.size()],
                                         pay_contract_key,
                                         walletName));
    }
    static constexpr auto init_balance = 1000;
    for(cbdc::parsec::pybench_wallet& wal : wallets) {
        auto res = wal.init(init_balance, [&](bool ret) {
            if(!ret) {
                init_error = true;
            } else {
                init_count++;
            }
        });
        if(!res) {
            init_error = true;
            break;
        }
    }

    constexpr uint64_t timeout = 300;
    constexpr auto wait_time = std::chrono::seconds(1);
    for(size_t count = 0;
        init_count < n_wallets && !init_error && count < timeout;
        count++) {
        std::this_thread::sleep_for(wait_time);
    }
    if(init_count < n_wallets || init_error) {
        log->error("Error initializing accounts");
        return 1;
    }
    // WALLETS INITIALIZED

    // RUN A BENCH TEST
    auto pay_queue = cbdc::blocking_queue<size_t>();
    auto pay_times = std::vector<std::atomic<uint64_t>>(n_wallets);
    for(size_t i = 0; i < n_wallets; i++) {
        pay_queue.push(i);
        pay_times[i] = std::chrono::high_resolution_clock::now()
                           .time_since_epoch()
                           .count();
    }

    auto rng_seed
        = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto rng = std::default_random_engine(rng_seed);
    auto dist = std::uniform_int_distribution<size_t>(0, wallets.size() - 1);
    auto running_test = std::atomic_bool(true);
    auto in_flight = std::atomic<size_t>();
    auto total_tx = std::atomic<size_t>(0);

    auto thread_count = (n_wallets < std::thread::hardware_concurrency()
                             ? n_wallets
                             : std::thread::hardware_concurrency());
    log->trace("Thread count:", thread_count);
    static constexpr auto ns_per_ms = 1000000;
    static constexpr auto pay_amount = 10;
    auto threads = std::vector<std::thread>();
    for(size_t i = 0; i < thread_count; i++) {
        auto t = std::thread([&]() {
            size_t from{};
            while(pay_queue.pop(from)) {
                size_t to{};
                do {
                    to = dist(rng);
                } while(from == to);
                auto to_key = wallets[to].get_account_key();

                log->trace("Paying from",
                           wallets[from].get_account_key().c_str(),
                           "to",
                           wallets[to].get_account_key().c_str());
                auto tx_start = std::chrono::high_resolution_clock::now();
                pay_times[from] = tx_start.time_since_epoch().count();

                in_flight++;
                auto res = wallets[from].pay(
                    to_key,
                    pay_amount,
                    [&, tx_start, from, to](bool ret) {
                        if(!ret) {
                            log->fatal("Pay request error");
                        }
                        auto tx_end
                            = std::chrono::high_resolution_clock::now();
                        const auto tx_delay = tx_end - tx_start;
                        log->trace("Done paying from",
                                   wallets[from].get_account_key().c_str(),
                                   "to",
                                   wallets[to].get_account_key().c_str(),
                                   " Delay: ",
                                   tx_delay.count() / ns_per_ms,
                                   "ms.");
                        total_tx++;
                        if(running_test) {
                            pay_queue.push(from);
                        }
                        in_flight--;
                    });
                if(!res) {
                    log->fatal("Pay request failed");
                }
            }
        });
        threads.emplace_back(std::move(t));
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    constexpr auto test_duration = std::chrono::minutes(1);
    while(in_flight > 0 || running_test) {
        auto now = std::chrono::high_resolution_clock::now();
        uint64_t now_count = now.time_since_epoch().count();
        for(auto& pay_time : pay_times) {
            constexpr uint64_t nano_factor = 1000000000;
            constexpr uint64_t max_delay = timeout * nano_factor;
            auto payt = pay_time.load();
            if(payt > now_count) {
                continue;
            }
            auto delay = now_count - payt;
            if(delay > max_delay) {
                log->fatal("Pay request timed out");
            }
        }
        if(now - start_time > test_duration) {
            running_test = false;
        }
        std::this_thread::sleep_for(wait_time);
    }

    log->trace("Joining thread");
    pay_queue.clear();
    for(auto& t : threads) {
        t.join();
    }

    log->trace("Checking balances");

    auto tot = std::atomic<uint32_t>{};
    init_count = 0;
    init_error = false;
    log->trace("AGGREGATING VALUES:");
    for(size_t i = 0; i < n_wallets; i++) {
        cbdc::parsec::get_row(
            broker,
            wallets[i].get_account_key(),
            [&](cbdc::parsec::broker::interface::try_lock_return_type res) {
                if(std::holds_alternative<
                       cbdc::parsec::runtime_locking_shard::value_type>(res)) {
                    auto found = std::get<
                        cbdc::parsec::runtime_locking_shard::value_type>(res);
                    tot += std::stoi(found.c_str());
                    log->trace(wallets[i].get_account_key().c_str(),
                               " balance: ",
                               found.c_str());
                    init_count++;
                } else {
                    log->fatal("Getting account returned error");
                }
            });
    }

    for(size_t count = 0;
        init_count < n_wallets && !init_error && count < timeout;
        count++) {
        std::this_thread::sleep_for(wait_time);
    }

    if(init_count < n_wallets || init_error) {
        log->error("Error updating balances");
        return 2;
    }

    if(tot != init_balance * n_wallets) {
        log->error("End balance does not equal start balance");
        log->error("Start Balance:",
                   init_balance * n_wallets,
                   "End Balance:",
                   tot);
        return 3;
    }

    log->trace("Balances check out");
    log->trace(total_tx, "total transactions recorded in 1 minute.");
    running = false;
    return 0;
}
