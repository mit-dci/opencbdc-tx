// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "common/config.hpp"
#include "common/logging.hpp"
#include "coordinator/client.hpp"
#include "locking_shard/status_client.hpp"
#include "network/connection_manager.hpp"
#include "sentinel/client.hpp"
#include "sentinel/format.hpp"
#include "serialization/format.hpp"
#include "transaction/messages.hpp"
#include "transaction/wallet.hpp"

#include <csignal>
#include <iostream>

auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    if(args.size() < 3) {
        std::cerr << "Usage: " << args[0] << " <config file> <gen ID>"
                  << std::endl;
        return -1;
    }

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        std::cerr << "Error loading config file: "
                  << std::get<std::string>(cfg_or_err) << std::endl;
        return -1;
    }
    auto cfg = std::get<cbdc::config::options>(cfg_or_err);

    auto gen_id = std::stoull(args[2]);
    auto logger
        = std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::info);

    auto sha2_impl = SHA256AutoDetect();
    logger->info("using sha2: ", sha2_impl);

    auto our_sentinel = gen_id % cfg.m_sentinel_endpoints.size();

    auto engine = std::default_random_engine();
    auto invalid_dist = std::bernoulli_distribution(cfg.m_invalid_rate);
    auto fixed_dist = std::bernoulli_distribution(cfg.m_fixed_tx_rate);

    auto wallet = cbdc::transaction::wallet();

    // Optionally Pre-seed wallet with deterministic UTXOs
    if(cfg.m_seed_from != cfg.m_seed_to) {
        auto [range_start, range_end]
            = cbdc::config::loadgen_seed_range(cfg, gen_id);

        bool ret = wallet.seed(cfg.m_seed_privkey.value(),
                               cfg.m_seed_value,
                               range_start,
                               range_end);
        if(!ret) {
            logger->error("Initial seed failed");
            return -1;
        }
        logger->info("Using pre-seeded wallet with UTXOs",
                     range_start,
                     "-",
                     range_end);
    }

    // Only mint when not pre-seeding
    if(cfg.m_seed_from == cfg.m_seed_to) {
        auto coordinator_client
            = cbdc::coordinator::rpc::client(cfg.m_coordinator_endpoints[0]);
        if(!coordinator_client.init()) {
            logger->error("Failed to connect to coordinator");
            return -1;
        }

        auto mint_tx = wallet.mint_new_coins(cfg.m_initial_mint_count,
                                             cfg.m_initial_mint_value);

        auto mint_successful = std::promise<bool>();
        auto mint_successful_fut = mint_successful.get_future();
        auto send_successful = coordinator_client.execute_transaction(
            cbdc::transaction::compact_tx(mint_tx),
            [&](std::optional<bool> resp) {
                if(!resp.has_value()) {
                    mint_successful.set_value(false);
                    return;
                }
                mint_successful.set_value(resp.value());
            });

        if(!send_successful) {
            logger->error("Failed to send mint TX to coordinator");
            return -1;
        }

        logger->info("Waiting for mint confirmation");
        auto mint_result = mint_successful_fut.get();
        if(!mint_result) {
            logger->error("Mint TX failed");
            return -1;
        }

        wallet.confirm_transaction(mint_tx);
        logger->info("Mint confirmed");
    }

    static constexpr auto lookup_timeout = std::chrono::milliseconds(5000);
    auto status_client = cbdc::locking_shard::rpc::status_client(
        cfg.m_locking_shard_readonly_endpoints,
        cfg.m_shard_ranges,
        lookup_timeout);
    if(!status_client.init()) {
        logger->error("Failed to connect to shard read-only endpoints");
        return -1;
    }

    auto sentinel_client
        = cbdc::sentinel::rpc::client({cfg.m_sentinel_endpoints[our_sentinel]},
                                      logger);
    if(!sentinel_client.init()) {
        logger->error("Failed to connect to sentinel");
        return -1;
    }

    auto confirmed_txs = std::queue<cbdc::transaction::full_tx>();
    auto confirmed_txs_mut = std::mutex();

    std::ofstream latency_log("tx_samples_" + std::to_string(gen_id) + ".txt");

    auto second_conf_queue = cbdc::blocking_queue<cbdc::hash_t>();
    auto second_conf_thrs = std::vector<std::thread>();

    static std::atomic_bool running{true};

    static constexpr auto n_second_conf_thrs = 100;
    for(auto i = 0; i < n_second_conf_thrs; i++) {
        second_conf_thrs.emplace_back([&]() {
            while(running) {
                auto tx_id = cbdc::hash_t();
                auto res = second_conf_queue.pop(tx_id);
                if(!res) {
                    continue;
                }
                auto conf = status_client.check_tx_id(tx_id);
                if(!conf) {
                    logger->warn(cbdc::to_string(tx_id), "no response");
                } else if(!*conf) {
                    logger->warn(cbdc::to_string(tx_id), "wasn't confirmed");
                }
            }
        });
    }

    constexpr auto send_amt = 5;

    uint64_t gen_avg{};
    auto gen_thread = std::thread([&]() {
        while(running) {
            // Determine if we should attempt to send a double-spending
            // transaction
            bool send_invalid{false};
            if(cfg.m_invalid_rate > 0.0) {
                send_invalid = invalid_dist(engine);
            }

            // Determine if we should attempt to send a fixed-size transaction
            bool send_fixed{false};
            if(cfg.m_fixed_tx_mode && cfg.m_fixed_tx_rate > 0.0) {
                send_fixed = fixed_dist(engine);
            }

            auto tx = std::optional<cbdc::transaction::full_tx>();
            // Try to send a double-spending transaction
            if(send_invalid) {
                std::lock_guard<std::mutex> l(confirmed_txs_mut);
                // Attempt to pop a previously confirmed transaction to re-send
                // (will now be a double-spend)
                if(!confirmed_txs.empty()) {
                    tx = std::move(confirmed_txs.front());
                    confirmed_txs.pop();
                }
            }

            // tx is empty so there wasn't a double-spend available for us to
            // send. Try to send a new (valid) transaction instead.
            if(!tx) {
                // If we're sending fixed-size transactions, attempt to
                // generate a fixed-size transaction.
                auto gen_s = std::chrono::high_resolution_clock::now();
                if(send_fixed) {
                    tx = wallet.send_to(cfg.m_input_count,
                                        cfg.m_output_count,
                                        wallet.generate_key(),
                                        true);
                } else {
                    // If using fixed TX mode, the fallback in/out count
                    // should be 2/2
                    if(cfg.m_fixed_tx_mode && cfg.m_fixed_tx_rate > 0.0) {
                        tx = wallet.send_to(2, 2, wallet.generate_key(), true);
                    } else {
                        // Otherwise send a regular transaction and let the
                        // wallet determine the input/output count.
                        tx = wallet.send_to(send_amt,
                                            wallet.generate_key(),
                                            true);
                    }
                }
                auto gen_e = std::chrono::high_resolution_clock::now();
                auto gen_t = gen_e - gen_s;
                static constexpr auto average_factor = 0.1;
                gen_avg = static_cast<uint64_t>(
                    (static_cast<double>(gen_t.count()) * average_factor)
                    + (static_cast<double>(gen_avg) * (1.0 - average_factor)));
            } else {
                std::this_thread::sleep_for(std::chrono::nanoseconds(gen_avg));
            }

            // We couldn't send a double-spend or a newly generated valid
            // transaction, emit a warning and wait for confirmations.
            if(!tx) {
                logger->warn("Wallet out of outputs");
                // If we couldn't send any txs this loop because we're out of
                // spendable outputs, sleep the send thread for some time in
                // the hope of receiving confirmations. Note, this could also
                // be a condition variable signalled by the confirmation thread
                // instead.
                static constexpr auto send_delay = std::chrono::seconds(1);
                std::this_thread::sleep_for(send_delay);
                continue;
            }

            auto send_time = std::chrono::high_resolution_clock::now()
                                 .time_since_epoch()
                                 .count();

            auto res_cb = [&, txn = tx.value(), send_time = send_time](
                              cbdc::sentinel::rpc::client::result_type res) {
                auto tx_id = cbdc::transaction::tx_id(txn);
                if(!res.has_value()) {
                    logger->warn("Failure response from sentinel for",
                                 cbdc::to_string(tx_id));
                    wallet.confirm_inputs(txn.m_inputs);
                    return;
                }
                auto& sent_resp = res.value();
                if(sent_resp.m_tx_status
                   == cbdc::sentinel::tx_status::confirmed) {
                    wallet.confirm_transaction(txn);
                    second_conf_queue.push(tx_id);
                    auto now = std::chrono::high_resolution_clock::now()
                                   .time_since_epoch()
                                   .count();
                    const auto tx_delay = now - send_time;
                    latency_log << now << " " << tx_delay << "\n";
                    constexpr auto max_invalid = 100000;
                    if(cfg.m_invalid_rate > 0.0) {
                        std::lock_guard<std::mutex> l(confirmed_txs_mut);
                        if(confirmed_txs.size() < max_invalid) {
                            confirmed_txs.push(txn);
                        }
                    }
                } else {
                    logger->warn(cbdc::to_string(tx_id), "had error");
                    wallet.confirm_inputs(txn.m_inputs);
                    // TODO: in some cases we should retry the TX here
                }
            };

            if(!sentinel_client.execute_transaction(tx.value(),
                                                    std::move(res_cb))) {
                logger->error("Failure sending transaction to sentinel");
                wallet.confirm_inputs(tx.value().m_inputs);
            }
        }
    });

    std::signal(SIGINT, [](int /* sig */) {
        running = false;
    });

    while(running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    gen_thread.join();
    second_conf_queue.clear();
    for(auto& thr : second_conf_thrs) {
        thr.join();
    }

    return 0;
}
