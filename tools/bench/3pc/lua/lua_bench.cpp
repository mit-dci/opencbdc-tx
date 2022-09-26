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
    auto cfg = cbdc::threepc::read_config(argc - 1, argv);
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

    auto pay_contract = cbdc::buffer();
    pay_contract
        = cbdc::buffer::from_hex(
              "1b4c7561540019930d0a1a0a040808785600000000000000000000002877400"
              "1808ac4010008d48b0000058e0001060381030080010000c40003060f000405"
              "0f0003040f0002030f0001020f000001cf0000000f000801cf8000000f00090"
              "1cf0001000f000a01cf8001000f000b01cf0002000f000c018b0000090b0100"
              "00c40002030f000e020f000d018b00000c0b0100018b0100020b020003c4000"
              "4020f000f018b0000100b0100008b0100040b02000fc40004018b0000030b01"
              "000eba000200380100808b00001103010900c40002018b0000020b01000d3a0"
              "10100380100808b00001103810900c40002018b000002c0007f00b80700808b"
              "0000090b010001c40002030f0015020f0014018b0000140b010002a2000102a"
              "e0002060f0014018b00000d0b010002a3000102ae0002070f000d0138000080"
              "0f8001168b00000395000180af0080060f000e018b00000b0b0100008b01000"
              "d0b02000e8b0200010b0300148b030015c5000700c6000000c7000100970485"
              "66726f6d0483746f048676616c7565048973657175656e63650484736967048"
              "7737472696e670487756e7061636b0492633332206333322049382049382063"
              "363404906765745f6163636f756e745f6b6579048c6765745f6163636f756e7"
              "4048d7061636b5f6163636f756e7404907570646174655f6163636f756e7473"
              "048c7369675f7061796c6f6164048d66726f6d5f62616c616e6365048966726"
              "f6d5f73657104887061796c6f6164048a636865636b5f73696704866572726f"
              "72049873657175656e6365206e756d62657220746f6f206c6f770495696e737"
              "56666696369656e742062616c616e6365048b746f5f62616c616e6365048774"
              "6f5f736571008100000085808d91010003880f8000018b00000000010000b50"
              "002000f0002018b000002c8000200c700010083048f6163636f756e745f7072"
              "6566697804896163636f756e745f048c6163636f756e745f6b6579810000008"
              "08080808080939c0100049d8b00000100010000c40002020f0000018b000003"
              "8e0001040b010000c40002020f0002018b0000058e0001060b010002c400020"
              "2c0007f00b80400808b0000058e000109030105008b010002c40003030f0008"
              "020f0007018b0000070b010008c60003008180ff7f0181ff7fc6000300c7000"
              "1008b048c6163636f756e745f6b657904906765745f6163636f756e745f6b65"
              "79048d6163636f756e745f64617461048a636f726f7574696e6504867969656"
              "c640487737472696e6704846c656e04906163636f756e745f62616c616e6365"
              "04916163636f756e745f73657175656e63650487756e7061636b04864938204"
              "938810000008080808080809ea00400098b0b02000080020100440202028b02"
              "00018e020502038301008003020000040300c40204021000040547020100840"
              "4906765745f6163636f756e745f6b65790487737472696e6704857061636b04"
              "86493820493881000000808080808080a2a906000b9413030000520000000f0"
              "000060b0300018b03000000040000800401000005020044030501bc810200b8"
              "0200800b0300018b030000000403008004040000050500440305010b0300004"
              "803020047030100830484726574048d7061636b5f6163636f756e7400810000"
              "00808080808080abad030008898b0100008e010301030201008002000000030"
              "10080030200c5010500c6010000c7010100830487737472696e670485706163"
              "6b048a63333220493820493881000000808080808080808080")
              .value();

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
