// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "3pc/agent/client.hpp"
#include "3pc/broker/impl.hpp"
#include "3pc/directory/impl.hpp"
#include "3pc/runtime_locking_shard/client.hpp"
#include "3pc/ticket_machine/client.hpp"
#include "3pc/util.hpp"
#include "crypto/sha256.h"
#include "wallet.hpp"

#include <lua.hpp>
#include <random>
#include <thread>

auto main(int argc, char** argv) -> int {
    auto log
        = std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::warn);

    auto sha2_impl = SHA256AutoDetect();
    log->info("using sha2: ", sha2_impl);

    if(argc < 2) {
        log->error("Not enough arguments");
        return 1;
    }
    auto cfg = cbdc::threepc::read_config(argc - 2, argv);
    if(!cfg.has_value()) {
        log->error("Error parsing options");
        return 1;
    }

    auto args = cbdc::config::get_args(argc, argv);
    auto n_wallets = std::stoull(args.back());
    if(n_wallets < 2) {
        log->error("Must be at least two threads");
        return 1;
    }

    log->trace("Connecting to shards");
    auto shards = std::vector<
        std::shared_ptr<cbdc::threepc::runtime_locking_shard::interface>>();
    for(const auto& shard_ep : cfg->m_shard_endpoints) {
        auto client = std::make_shared<
            cbdc::threepc::runtime_locking_shard::rpc::client>(
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
        = std::make_shared<cbdc::threepc::ticket_machine::rpc::client>(
            std::vector<cbdc::network::endpoint_t>{
                cfg->m_ticket_machine_endpoints});
    if(!ticketer->init()) {
        log->error("Error connecting to ticket machine");
        return 1;
    }
    log->trace("Connected to ticket machine");

    auto directory
        = std::make_shared<cbdc::threepc::directory::impl>(shards.size());
    auto broker = std::make_shared<cbdc::threepc::broker::impl>(
        std::numeric_limits<size_t>::max(),
        shards,
        ticketer,
        directory,
        log);

    auto contract_file = args[args.size() - 2];
    auto pay_contract = cbdc::buffer();
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dofile(L, contract_file.c_str());
    lua_getglobal(L, "gen_bytecode");
    if(lua_pcall(L, 0, 1, 0) != 0) {
        log->error("Contract bytecode generation failed, with error:",
                   lua_tostring(L, -1));
        return 1;
    }
    pay_contract = cbdc::buffer::from_hex(lua_tostring(L, -1)).value();

    auto pay_keys = std::vector<cbdc::buffer>();
    auto init_count = std::atomic<size_t>();
    auto init_error = std::atomic_bool{false};
    for(size_t i = 0; i < n_wallets; i++) {
        auto pay_contract_key = cbdc::buffer();
        pay_contract_key.append("pay", 3);
        pay_contract_key.append(&i, sizeof(i));
        pay_keys.push_back(pay_contract_key);

        log->info("Inserting pay contract", i);
        auto ret = cbdc::threepc::put_row(
            broker,
            pay_contract_key,
            pay_contract,
            [&](bool res) {
                if(!res) {
                    init_error = true;
                } else {
                    log->info("Inserted pay contract", i);
                    init_count++;
                }
            });
        if(!ret) {
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
        log->error("Error adding pay contract");
        return 2;
    }

    auto agents
        = std::vector<std::shared_ptr<cbdc::threepc::agent::rpc::client>>();
    for(auto& a : cfg->m_agent_endpoints) {
        auto agent = std::make_shared<cbdc::threepc::agent::rpc::client>(
            std::vector<cbdc::network::endpoint_t>{a});
        if(!agent->init()) {
            log->error("Error connecting to agent");
            return 1;
        }
        agents.emplace_back(agent);
    }

    auto wallets = std::vector<cbdc::threepc::account_wallet>();
    for(size_t i = 0; i < n_wallets; i++) {
        auto agent_idx = i % agents.size();
        wallets.emplace_back(log, broker, agents[agent_idx], pay_keys[i]);
    }

    constexpr auto init_balance = 10000;
    init_count = 0;
    init_error = false;
    for(size_t i = 0; i < n_wallets; i++) {
        auto res = wallets[i].init(init_balance, [&](bool ret) {
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

    for(size_t count = 0;
        init_count < n_wallets && !init_error && count < timeout;
        count++) {
        std::this_thread::sleep_for(wait_time);
    }

    if(init_count < n_wallets || init_error) {
        log->error("Error initializing accounts");
        return 1;
    }

    log->info("Added new accounts");

    std::mutex samples_mut;
    auto samples_file = std::ofstream(
        "tx_samples_" + std::to_string(cfg->m_component_id) + ".txt");
    if(!samples_file.good()) {
        log->error("Unable to open samples file");
        return 1;
    }

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
    auto running = std::atomic_bool(true);
    auto in_flight = std::atomic<size_t>();

    auto thread_count = std::thread::hardware_concurrency();
    auto threads = std::vector<std::thread>();
    for(size_t i = 0; i < thread_count; i++) {
        auto t = std::thread([&]() {
            size_t from{};
            while(pay_queue.pop(from)) {
                size_t to{};
                do {
                    to = dist(rng);
                } while(from == to);
                auto to_key = wallets[to].get_pubkey();
                log->trace("Paying from", from, "to", to);
                auto tx_start = std::chrono::high_resolution_clock::now();
                pay_times[from] = tx_start.time_since_epoch().count();
                in_flight++;
                auto res = wallets[from].pay(
                    to_key,
                    1,
                    [&, tx_start, from, to](bool ret) {
                        if(!ret) {
                            log->fatal("Pay request error");
                        }
                        auto tx_end
                            = std::chrono::high_resolution_clock::now();
                        const auto tx_delay = tx_end - tx_start;
                        auto out_buf = std::stringstream();
                        out_buf << tx_end.time_since_epoch().count() << " "
                                << tx_delay.count() << "\n";
                        auto out_str = out_buf.str();
                        {
                            std::unique_lock l(samples_mut);
                            samples_file << out_str;
                        }
                        log->trace("Done paying from", from, "to", to);
                        if(running) {
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
    constexpr auto test_duration = std::chrono::minutes(5);
    while(in_flight > 0 || running) {
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
            running = false;
        }
        std::this_thread::sleep_for(wait_time);
    }

    log->trace("Joining thread");
    pay_queue.clear();
    for(auto& t : threads) {
        t.join();
    }

    log->trace("Checking balances");

    auto tot = std::atomic<uint64_t>{};
    init_count = 0;
    init_error = false;
    for(size_t i = 0; i < n_wallets; i++) {
        auto res = wallets[i].update_balance([&, i](bool ret) {
            if(!ret) {
                init_error = true;
            } else {
                tot += wallets[i].get_balance();
                init_count++;
            }
        });
        if(!res) {
            init_error = true;
            break;
        }
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
        return 3;
    }

    log->trace("Checked balances");

    return 0;
}
