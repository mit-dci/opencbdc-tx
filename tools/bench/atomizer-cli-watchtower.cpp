// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/atomizer/atomizer/atomizer_raft.hpp"
#include "uhs/atomizer/atomizer/block.hpp"
#include "uhs/atomizer/atomizer/format.hpp"
#include "uhs/atomizer/watchtower/client.hpp"
#include "uhs/atomizer/watchtower/watchtower.hpp"
#include "uhs/sentinel/client.hpp"
#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/wallet.hpp"
#include "util/common/config.hpp"
#include "util/common/logging.hpp"
#include "util/network/connection_manager.hpp"
#include "util/serialization/format.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

auto send_tx_to_atomizer(const cbdc::transaction::compact_tx& tx,
                         const uint32_t height)
    -> cbdc::atomizer::tx_notify_request {
    cbdc::atomizer::tx_notify_request msg;
    msg.m_tx = tx;
    msg.m_block_height = height;
    for(uint32_t i = 0; i < msg.m_tx.m_inputs.size(); i++) {
        msg.m_attestations.insert(i);
    }

    return msg;
}

auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    if(args.size() < 4) {
        std::cerr << "Usage: " << args[0]
                  << " <config file> <atomizer-cli id> <sign txs> "
                     "[<trace(default: 1)>]"
                  << std::endl;
        return 0;
    }

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        std::cerr << "Error loading config file: "
                  << std::get<std::string>(cfg_or_err) << std::endl;
        return -1;
    }
    auto cfg = std::get<cbdc::config::options>(cfg_or_err);

    const auto cli_id = std::stoull(args[2]);

    constexpr uint32_t watchtower_batch_size{5000};

    const auto sign_txs = static_cast<bool>(std::stoull(args[3]));

    auto loglevel = cbdc::logging::log_level::trace;
    if(args.size() == 4) {
        loglevel = cbdc::logging::log_level::info;
    }
    auto log = std::make_shared<cbdc::logging::log>(loglevel);

    std::string sha2_impl(SHA256AutoDetect());
    log->debug("using sha2:", sha2_impl);

    if(!(cfg.m_window_size > 0)) {
        log->fatal(
            "Please specify a valid window size in your config file (>0).");
        return -1;
    }

    auto engine = std::default_random_engine();
    auto invalid_dist = std::bernoulli_distribution(cfg.m_invalid_rate);
    auto fixed_dist = std::bernoulli_distribution(cfg.m_fixed_tx_rate);

    cbdc::network::connection_manager atomizer_network;

    atomizer_network.cluster_connect(cfg.m_atomizer_endpoints, false);
    if(!atomizer_network.connected_to_one()) {
        log->warn("Failed to connect to any atomizers");
    }

    static std::atomic_bool atomizer_network_running = true;
    std::thread atomizer_handler_thread([&]() {
        while(atomizer_network_running) {
            [[maybe_unused]] auto msgs = atomizer_network.handle_messages();
        }
    });

    auto sentinel_client = std::unique_ptr<cbdc::sentinel::rpc::client>();

    if(sign_txs) {
        // TODO: load balancing strategies other than round-robin, backpressure
        sentinel_client = std::make_unique<cbdc::sentinel::rpc::client>(
            std::vector<cbdc::network::endpoint_t>(cfg.m_sentinel_endpoints),
            log);
        if(!sentinel_client->init()) {
            log->error("Error connecting to sentinels");
            return -1;
        }
    }

    auto our_watchtower = cli_id % cfg.m_watchtower_client_endpoints.size();

    auto watchtower_client = std::make_shared<cbdc::watchtower::async_client>(
        cfg.m_watchtower_client_endpoints[our_watchtower]);

    if(!watchtower_client->init()) {
        log->warn("Failed to connect to watchtower.");
    }

    auto blocking_watchtower_client
        = std::make_shared<cbdc::watchtower::blocking_client>(
            cfg.m_watchtower_client_endpoints[our_watchtower]);

    if(!blocking_watchtower_client->init()) {
        log->warn("Failed to connect to watchtower.");
    }

    cbdc::transaction::wallet wal(log);

    // Optionally Pre-seed wallet with deterministic UTXOs
    if(cfg.m_seed_from != cfg.m_seed_to) {
        auto [range_start, range_end]
            = cbdc::config::loadgen_seed_range(cfg, cli_id);

        bool ret = wal.seed(cfg.m_seed_privkey.value(),
                            cfg.m_seed_value,
                            range_start,
                            range_end);
        if(!ret) {
            log->error("Initial seed failed");
            return -1;
        }
        log->info("Using pre-seeded wallet with UTXOs",
                  range_start,
                  "-",
                  range_end);
    }

    bool block_changed{false};
    static std::condition_variable block_cv;
    std::mutex block_mut;

    std::mutex txs_mut;
    std::unordered_map<cbdc::hash_t,
                       std::pair<uint64_t, uint32_t>,
                       cbdc::hashing::const_sip_hash<cbdc::hash_t>>
        txs;
    std::unordered_map<cbdc::hash_t,
                       cbdc::transaction::full_tx,
                       cbdc::hashing::const_sip_hash<cbdc::hash_t>>
        pending_txs;
    auto confirmed_txs = std::queue<cbdc::transaction::full_tx>();

    static std::atomic_bool running = true;
    uint64_t best_watchtower_height = 0;

    std::ofstream latency_log("tx_samples_" + std::to_string(cli_id) + ".txt");

    assert(latency_log.good());

    std::ofstream utxo_set_log("utxo_set_size_" + std::to_string(cli_id)
                               + ".txt");
    assert(utxo_set_log.good());

    watchtower_client->set_status_update_handler(
        [&](std::shared_ptr<cbdc::watchtower::status_request_check_success>&&
                res) {
            size_t confirmed = 0;
            size_t tx_rejected_errs = 0;
            size_t internal_errs = 0;
            size_t invalid_input_errs = 0;

            const auto& states = res->states();

            const auto now = std::chrono::high_resolution_clock::now()
                                 .time_since_epoch()
                                 .count();
            for(const auto& tx_id : states) {
                auto invalid = true;
                for(const auto& state : tx_id.second) {
                    switch(state.status()) {
                        case cbdc::watchtower::search_status::spent:
                        case cbdc::watchtower::search_status::unspent:
                            invalid = false;
                            break;
                        case cbdc::watchtower::search_status::tx_rejected:
                            tx_rejected_errs++;
                            break;
                        case cbdc::watchtower::search_status::internal_error:
                            internal_errs++;
                            break;
                        case cbdc::watchtower::search_status::invalid_input:
                            invalid_input_errs++;
                            break;
                        case cbdc::watchtower::search_status::no_history:
                            break;
                    }
                    best_watchtower_height = std::max(best_watchtower_height,
                                                      state.block_height());
                }
                {
                    std::lock_guard<std::mutex> lg(txs_mut);
                    const auto it = pending_txs.find(tx_id.first);
                    if(it != pending_txs.end()) {
                        if(!invalid) {
                            wal.confirm_transaction(it->second);
                            confirmed++;
                            const auto tx_it = txs.find(tx_id.first);
                            if(tx_it != txs.end()) {
                                const auto tx_delay
                                    = now - tx_it->second.first;
                                latency_log << now << " " << tx_delay << "\n";
                                txs.erase(tx_it);
                            }
                            if(cfg.m_invalid_rate > 0.0
                               && confirmed_txs.size() < cfg.m_window_size) {
                                confirmed_txs.push(std::move(it->second));
                            }
                            pending_txs.erase(it);
                        }
                    }
                }
            }
            size_t rebroadcast = 0;
            size_t in_flight{};
            {
                std::vector<cbdc::transaction::compact_tx> retry_txs;
                std::vector<cbdc::transaction::full_tx> retry_txs_full;
                {
                    std::lock_guard<std::mutex> lg(txs_mut);
                    for(auto&& tx : txs) {
                        if(tx.second.second + cfg.m_stxo_cache_depth
                           < best_watchtower_height) {
                            tx.second.second = best_watchtower_height;
                            const auto it = pending_txs.find(tx.first);
                            assert(it != pending_txs.end());
                            if(sign_txs) {
                                retry_txs_full.push_back(it->second);
                            } else {
                                retry_txs.emplace_back(it->second);
                            }
                        }
                    }
                    in_flight = pending_txs.size();
                }
                if(sign_txs) {
                    for(auto&& tx : retry_txs_full) {
                        sentinel_client->execute_transaction(
                            std::move(tx),
                            [](auto /*resp*/) {});
                        rebroadcast++;
                    }
                } else {
                    for(auto&& tx : retry_txs) {
                        auto resend_pkt
                            = send_tx_to_atomizer(tx, best_watchtower_height);
                        if(!atomizer_network.send_to_one(
                               cbdc::atomizer::request{resend_pkt})) {
                            log->error("Failed to resend tx to atomizer.");
                        }
                        rebroadcast++;
                    }
                }
            }

            const auto n_txos = wal.count();
            utxo_set_log << now << "\t" << n_txos << "\n";

            log->info("Watchtower responded. Best block height:",
                      best_watchtower_height,
                      "confirmed:",
                      confirmed,
                      ", Tx rejected errors:",
                      tx_rejected_errs,
                      ", internal errors:",
                      internal_errs,
                      ", invalid input errors:",
                      invalid_input_errs,
                      ", rebroadcast:",
                      rebroadcast,
                      ", in-flight:",
                      in_flight,
                      ", UTXOs: ",
                      n_txos,
                      " (",
                      n_txos * sizeof(cbdc::transaction::input),
                      " bytes)");
            {
                std::unique_lock<std::mutex> lck(block_mut);
                block_changed = true;
            }
            block_cv.notify_all();
        });

    // Only mint when not using pre-seeded wallets
    if(cfg.m_seed_from == cfg.m_seed_to) {
        const auto& mint_tx = wal.mint_new_coins(cfg.m_initial_mint_count,
                                                 cfg.m_initial_mint_value);

        cbdc::atomizer::tx_notify_request msg;

        msg.m_tx = cbdc::transaction::compact_tx(mint_tx);
        msg.m_block_height
            = blocking_watchtower_client->request_best_block_height()
                  ->height();

        {
            std::lock_guard<std::mutex> lg(txs_mut);
            const auto now = std::chrono::high_resolution_clock::now()
                                 .time_since_epoch()
                                 .count();
            txs.insert({msg.m_tx.m_id, {now, msg.m_block_height}});
            pending_txs.insert({cbdc::transaction::tx_id(mint_tx), mint_tx});
        }

        while(wal.balance() < 1) {
            auto bwc = cbdc::watchtower::blocking_client{
                cfg.m_watchtower_client_endpoints[our_watchtower]};
            if(!bwc.init()) {
                log->warn("Failed to connect to watchtower.");
            }
            if(atomizer_network.send_to_one(cbdc::atomizer::request{msg})) {
                log->info("Sent mint TX to atomizer. ID:",
                          cbdc::to_string(cbdc::transaction::tx_id(mint_tx)),
                          "h:",
                          msg.m_block_height);
            } else {
                log->error("Failed to send mint TX to atomizer. ID:",
                           cbdc::to_string(cbdc::transaction::tx_id(mint_tx)),
                           "h:",
                           msg.m_block_height);
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(cfg.m_target_block_interval)
                * cfg.m_stxo_cache_depth);
            cbdc::transaction::compact_tx ctx{mint_tx};
            auto out_id
                = cbdc::transaction::calculate_uhs_id(ctx.m_outputs[0]);
            watchtower_client->request_status_update(
                cbdc::watchtower::status_update_request{
                    {{ctx.m_id, {out_id}}}});
            static constexpr auto mint_retry_delay
                = std::chrono::milliseconds(1000);
            std::this_thread::sleep_for(mint_retry_delay);
        }
    }

    if(sign_txs) {
        atomizer_network_running = false;
        atomizer_network.close();
        atomizer_handler_thread.join();
    }

    std::signal(SIGINT, [](int /* signal */) {
        running = false;
        atomizer_network_running = false;
        block_cv.notify_all();
    });

    size_t count = 0;
    uint32_t batch_counter = 0;
    uint32_t best_height
        = blocking_watchtower_client->request_best_block_height()->height();
    std::chrono::nanoseconds total_time{};
    std::chrono::nanoseconds check_time{};
    std::chrono::nanoseconds gen_time{};
    std::chrono::nanoseconds add_time{};
    std::chrono::nanoseconds send_time{};
    uint64_t gen_avg{};
    while(running) {
        static constexpr auto send_amount = 5;
        const auto start_time = std::chrono::high_resolution_clock::now();

        auto count_in_flight = [&]() {
            std::lock_guard<std::mutex> lg(txs_mut);
            return pending_txs.size();
        };

        while(running
              && ((wal.balance() < send_amount && !cfg.m_fixed_tx_mode)
                  || (cfg.m_fixed_tx_mode
                      && (wal.count() < cfg.m_input_count
                          || wal.balance() / cfg.m_output_count == 0))
                  || (count_in_flight() >= cfg.m_window_size
                      && cfg.m_window_size > 0))) {
            // Wait for previous txs to confirm
            std::unordered_map<cbdc::hash_t,
                               std::vector<cbdc::hash_t>,
                               cbdc::hashing::const_sip_hash<cbdc::hash_t>>
                key_uhs_ids;
            {
                std::lock_guard<std::mutex> lck(txs_mut);
                key_uhs_ids.reserve(pending_txs.size());
                for(const auto& it : pending_txs) {
                    cbdc::transaction::compact_tx ctx{it.second};
                    std::vector<cbdc::hash_t> uhs_ids{};
                    std::transform(
                        ctx.m_outputs.begin(),
                        ctx.m_outputs.end(),
                        std::back_inserter(uhs_ids),
                        [](const cbdc::transaction::compact_output& p) {
                            return cbdc::transaction::calculate_uhs_id(p);
                        });

                    key_uhs_ids.emplace(std::make_pair(ctx.m_id, uhs_ids));
                }
            }
            watchtower_client->request_status_update(
                cbdc::watchtower::status_update_request{key_uhs_ids});
            log->info("Waiting for watchtower... (in-flight:",
                      count_in_flight(),
                      ")");
            {
                std::unique_lock<std::mutex> lck(block_mut);
                block_cv.wait(lck, [&]() {
                    return block_changed || !running;
                });
                if(block_changed) {
                    block_changed = false;
                }
            }
        }

        if(!running) {
            break;
        }

        bool send_invalid{false};
        if(cfg.m_invalid_rate > 0.0) {
            send_invalid = invalid_dist(engine);
        }

        bool send_fixed{false};
        if(cfg.m_fixed_tx_mode && cfg.m_fixed_tx_rate > 0.0) {
            send_fixed = fixed_dist(engine);
        }

        auto pay_tx = cbdc::transaction::full_tx();
        const auto gen_start_time = std::chrono::high_resolution_clock::now();
        if(send_invalid) {
            std::lock_guard<std::mutex> lg(txs_mut);
            if(confirmed_txs.empty()) {
                log->debug("Not enough confirmed TXs to send an invalid TX");
                continue;
            }
            pay_tx = std::move(confirmed_txs.front());
            confirmed_txs.pop();
        } else {
            auto gen_s = std::chrono::high_resolution_clock::now();
            if(send_fixed) {
                auto pay_tx_res = wal.send_to(cfg.m_input_count,
                                              cfg.m_output_count,
                                              wal.generate_key(),
                                              sign_txs);
                if(!pay_tx_res.has_value()) {
                    log->debug("Couldn't generate a TX with",
                               cfg.m_input_count,
                               "inputs and",
                               cfg.m_output_count,
                               "outputs");
                    continue;
                }
                pay_tx = std::move(pay_tx_res.value());
            } else {
                std::optional<cbdc::transaction::full_tx> pay_tx_res;
                if(cfg.m_fixed_tx_mode && cfg.m_fixed_tx_rate > 0.0) {
                    pay_tx_res
                        = wal.send_to(2, 2, wal.generate_key(), sign_txs);
                } else {
                    pay_tx_res = wal.send_to(send_amount,
                                             wal.generate_key(),
                                             sign_txs);
                }
                if(!pay_tx_res.has_value()) {
                    log->debug("Couldn't generate a TX");
                    continue;
                }
                pay_tx = std::move(pay_tx_res.value());
            }
            auto gen_e = std::chrono::high_resolution_clock::now();
            auto gen_t = gen_e - gen_s;
            static constexpr auto average_factor = 0.1;
            gen_avg = static_cast<uint64_t>(
                (static_cast<double>(gen_t.count()) * average_factor)
                + (static_cast<double>(gen_avg) * (1.0 - average_factor)));
        }

        const auto gen_end_time = std::chrono::high_resolution_clock::now();

        if(!send_invalid) {
            std::lock_guard<std::mutex> lg(txs_mut);
            const auto now = std::chrono::high_resolution_clock::now()
                                 .time_since_epoch()
                                 .count();
            txs.insert({cbdc::transaction::tx_id(pay_tx), {now, best_height}});
            pending_txs.insert({cbdc::transaction::tx_id(pay_tx), pay_tx});
        } else {
            std::this_thread::sleep_for(std::chrono::nanoseconds(gen_avg));
        }

        const auto add_end_time = std::chrono::high_resolution_clock::now();

        if(sign_txs) {
            sentinel_client->execute_transaction(std::move(pay_tx),
                                                 [](auto /* resp */) {});
        } else {
            auto send_pkt
                = send_tx_to_atomizer(cbdc::transaction::compact_tx(pay_tx),
                                      best_height);
            if(!atomizer_network.send_to_one(
                   cbdc::atomizer::request{send_pkt})) {
                log->info("Failed to send pay tx to atomizer. ID:",
                          cbdc::to_string(cbdc::transaction::tx_id(pay_tx)),
                          "h:",
                          best_height);
            };
        }

        const auto end_time = std::chrono::high_resolution_clock::now();
        count++;
        total_time += end_time - start_time;
        check_time += gen_start_time - start_time;
        gen_time += gen_end_time - gen_start_time;
        add_time += add_end_time - gen_end_time;
        send_time += end_time - add_end_time;

        if(++batch_counter == watchtower_batch_size) {
            std::unordered_map<cbdc::hash_t,
                               std::vector<cbdc::hash_t>,
                               cbdc::hashing::const_sip_hash<cbdc::hash_t>>
                key_uhs_ids;
            {
                std::lock_guard<std::mutex> lck(txs_mut);
                key_uhs_ids.reserve(pending_txs.size());
                for(const auto& it : pending_txs) {
                    cbdc::transaction::compact_tx ctx{it.second};
                    std::vector<cbdc::hash_t> uhs_ids{};
                    std::transform(
                        ctx.m_outputs.begin(),
                        ctx.m_outputs.end(),
                        std::back_inserter(uhs_ids),
                        [](const cbdc::transaction::compact_output& p) {
                            return cbdc::transaction::calculate_uhs_id(p);
                        });
                    key_uhs_ids.emplace(std::make_pair(ctx.m_id, uhs_ids));
                }
            }

            watchtower_client->request_status_update(
                cbdc::watchtower::status_update_request{key_uhs_ids});

            best_height
                = blocking_watchtower_client->request_best_block_height()
                      ->height();

            batch_counter = 0;
        }

        static constexpr auto delay_stats_print_interval = 10;
        if(std::chrono::duration_cast<std::chrono::seconds>(total_time).count()
           >= delay_stats_print_interval) {
            const auto n_txs = static_cast<float>(count);
            const auto total_delay
                = (static_cast<float>(total_time.count()) / n_txs)
                / std::pow(10, 9);

            const auto check_delay
                = (static_cast<float>(check_time.count()) / n_txs)
                / std::pow(10, 9);

            const auto gen_delay
                = (static_cast<float>(gen_time.count()) / n_txs)
                / std::pow(10, 9);

            const auto add_delay
                = (static_cast<float>(add_time.count()) / n_txs)
                / std::pow(10, 9);

            const auto send_delay
                = (static_cast<float>(send_time.count()) / n_txs)
                / std::pow(10, 9);

            log->info("Total:",
                      total_delay,
                      "s, ",
                      "Check: ",
                      check_delay,
                      "s, ",
                      "Gen: ",
                      gen_delay,
                      "s, ",
                      "Add: ",
                      add_delay,
                      "s, ",
                      "Send: ",
                      send_delay,
                      "s");

            count = 0;
            total_time = std::chrono::nanoseconds(0);
            check_time = std::chrono::nanoseconds(0);
            gen_time = std::chrono::nanoseconds(0);
            add_time = std::chrono::nanoseconds(0);
            send_time = std::chrono::nanoseconds(0);
        }
    }

    log->info("Shutting down...");

    if(!sign_txs) {
        atomizer_network.close();
        atomizer_handler_thread.join();
    }

    return 0;
}
