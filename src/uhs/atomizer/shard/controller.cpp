// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include "uhs/atomizer/atomizer/atomizer_raft.hpp"
#include "uhs/transaction/messages.hpp"

#include <utility>

namespace cbdc::shard {
    controller::controller(uint32_t shard_id,
                           config::options opts,
                           std::shared_ptr<logging::log> logger)
        : m_shard_id(shard_id),
          m_opts(std::move(opts)),
          m_logger(std::move(logger)),
          m_shard(m_opts.m_shard_ranges[shard_id]),
          m_archiver_client(m_opts.m_archiver_endpoints[0], m_logger) {}

    controller::~controller() {
        m_shard_network.close();
        m_atomizer_network.close();

        if(m_shard_server.joinable()) {
            m_shard_server.join();
        }
        if(m_atomizer_client.joinable()) {
            m_atomizer_client.join();
        }

        m_request_queue.clear();
        for(auto& t : m_handler_threads) {
            if(t.joinable()) {
                t.join();
            }
        }

        if(m_audit_thread.joinable()) {
            m_audit_thread.join();
        }
    }

    auto controller::init() -> bool {
        if(auto err_msg
           = m_shard.open_db(m_opts.m_shard_db_dirs[m_shard_id])) {
            m_logger->error("Failed to open shard DB for shard",
                            m_shard_id,
                            ". Got error:",
                            *err_msg);
            return false;
        }

        m_audit_log.open(m_opts.m_shard_audit_logs[m_shard_id],
                         std::ios::app | std::ios::out);
        if(!m_audit_log.good()) {
            m_logger->error("Failed to open audit log");
            return false;
        }

        if(!m_archiver_client.init()) {
            m_logger->warn("Failed to connect to archiver");
        }

        if(!m_watchtower_network.cluster_connect(
               m_opts.m_watchtower_internal_endpoints)) {
            m_logger->warn("Failed to connect to watchtowers.");
        }

        m_atomizer_network.cluster_connect(m_opts.m_atomizer_endpoints, false);
        if(!m_atomizer_network.connected_to_one()) {
            m_logger->warn("Failed to connect to any atomizers");
        }

        m_atomizer_client = m_atomizer_network.start_handler([&](auto&& pkt) {
            return atomizer_handler(std::forward<decltype(pkt)>(pkt));
        });

        constexpr auto max_wait = 3;
        for(size_t i = 0; i < max_wait && m_shard.best_block_height() < 1;
            i++) {
            m_logger->info("Waiting to sync with atomizer");
            constexpr auto wait_time = std::chrono::seconds(1);
            std::this_thread::sleep_for(wait_time);
        }

        if(m_shard.best_block_height() < 1) {
            m_logger->warn(
                "Shard still not syncronized with atomizer, starting anyway");
        }

        auto ss = m_shard_network.start_server(
            m_opts.m_shard_endpoints[m_shard_id],
            [&](auto&& pkt) {
                return server_handler(std::forward<decltype(pkt)>(pkt));
            });

        if(!ss.has_value()) {
            m_logger->error("Failed to establish shard server.");
            return false;
        }

        m_shard_server = std::move(ss.value());

        auto n_threads = std::thread::hardware_concurrency();
        for(size_t i = 0; i < n_threads; i++) {
            m_handler_threads.emplace_back([&]() {
                request_consumer();
            });
        }

        return true;
    }

    auto controller::server_handler(cbdc::network::message_t&& pkt)
        -> std::optional<cbdc::buffer> {
        m_request_queue.push(pkt);
        return std::nullopt;
    }

    auto controller::atomizer_handler(cbdc::network::message_t&& pkt)
        -> std::optional<cbdc::buffer> {
        auto maybe_blk = from_buffer<atomizer::block>(*pkt.m_pkt);
        if(!maybe_blk.has_value()) {
            m_logger->error("Invalid block packet");
            return std::nullopt;
        }

        auto& blk = maybe_blk.value();

        m_logger->info("Digesting block", blk.m_height, "...");

        // If the block is not contiguous, catch up by requesting
        // blocks from the archiver.
        while(!m_shard.digest_block(blk)) {
            m_logger->warn("Block",
                           blk.m_height,
                           "not contiguous with previous block",
                           m_shard.best_block_height());

            if(blk.m_height <= m_shard.best_block_height()) {
                break;
            }

            // Attempt to catch up to the latest block
            for(uint64_t i = m_shard.best_block_height() + 1; i < blk.m_height;
                i++) {
                const auto past_blk = m_archiver_client.get_block(i);
                if(past_blk) {
                    m_shard.digest_block(past_blk.value());
                    audit();
                } else {
                    m_logger->info("Waiting for archiver sync");
                    const auto wait_time = std::chrono::milliseconds(10);
                    std::this_thread::sleep_for(wait_time);
                    i--;
                    continue;
                }
            }
        }
        audit();

        m_logger->info("Digested block", blk.m_height);
        return std::nullopt;
    }

    void controller::request_consumer() {
        auto pkt = network::message_t();
        while(m_request_queue.pop(pkt)) {
            auto maybe_tx = from_buffer<transaction::compact_tx>(*pkt.m_pkt);
            if(!maybe_tx.has_value()) {
                m_logger->error("Invalid transaction packet");
                continue;
            }

            auto& tx = maybe_tx.value();

            m_logger->info("Digesting transaction", to_string(tx.m_id), "...");

            if(!transaction::validation::check_attestations(
                   tx,
                   m_opts.m_sentinel_public_keys,
                   m_opts.m_attestation_threshold)) {
                m_logger->warn("Received invalid compact transaction",
                               to_string(tx.m_id));
                continue;
            }

            auto res = m_shard.digest_transaction(std::move(tx));

            auto res_handler = overloaded{
                [&](const atomizer::tx_notify_request& msg) {
                    m_logger->info("Digested transaction",
                                   to_string(msg.m_tx.m_id));

                    m_logger->debug("Sending",
                                    msg.m_attestations.size(),
                                    "/",
                                    msg.m_tx.m_inputs.size(),
                                    "attestations...");
                    if(!m_atomizer_network.send_to_one(
                           atomizer::request{msg})) {
                        m_logger->error(
                            "Failed to transmit tx to atomizer. ID:",
                            to_string(msg.m_tx.m_id));
                    }
                },
                [&](const cbdc::watchtower::tx_error& err) {
                    m_logger->info("error for Tx:",
                                   to_string(err.tx_id()),
                                   err.to_string());
                    // TODO: batch errors into a single RPC
                    auto data = std::vector<cbdc::watchtower::tx_error>{err};
                    auto buf = make_shared_buffer(data);
                    m_watchtower_network.broadcast(buf);
                }};
            std::visit(res_handler, res);
        }
    }

    void controller::audit() {
        auto height = m_shard.best_block_height();
        if(m_opts.m_shard_audit_interval > 0
           && height % m_opts.m_shard_audit_interval != 0) {
            return;
        }

        auto snp = m_shard.get_snapshot();
        if(m_audit_thread.joinable()) {
            m_audit_thread.join();
        }
        m_audit_thread = std::thread([this, s = std::move(snp), height]() {
            auto range_summaries = m_shard.audit(s);
            auto buf = cbdc::buffer();
            buf.extend(sizeof(commitment_t));
            for(const auto& [bucket, summary] : range_summaries) {
                buf.clear();
                buf.append(summary.data(), summary.size());
                m_audit_log << height << " " << static_cast<int>(bucket) << " "
                            << buf.to_hex() << std::endl;
            }
            m_logger->info("Audit completed for", height);
        });
    }
}
