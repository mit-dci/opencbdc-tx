// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "twophase_client.hpp"

#include "uhs/transaction/messages.hpp"

namespace cbdc {
    twophase_client::twophase_client(
        const cbdc::config::options& opts,
        const std::shared_ptr<logging::log>& logger,
        const std::string& wallet_file,
        const std::string& client_file)
        : client(opts, logger, wallet_file, client_file),
          m_coordinator_client(opts.m_coordinator_endpoints[0]),
          m_shard_status_client(opts.m_locking_shard_readonly_endpoints,
                                opts.m_shard_ranges,
                                m_client_timeout),
          m_logger(logger),
          m_opts(opts) {}

    auto twophase_client::init_derived() -> bool {
        if(!m_coordinator_client.init()) {
            m_logger->warn("Failed to initialize coordinator client");
        }

        if(!m_shard_status_client.init()) {
            m_logger->warn("Failed to initialize shard status client");
        }

        return true;
    }

    auto twophase_client::sync() -> bool {
        auto success = true;

        auto txids = std::set<hash_t>();
        for(const auto& [tx_id, tx] : pending_txs()) {
            txids.insert(tx_id);
        }
        for(const auto& [tx_id, inp] : pending_inputs()) {
            txids.insert(tx_id);
        }

        for(const auto& tx_id : txids) {
            m_logger->debug("Requesting status of", to_string(tx_id));
            auto res = m_shard_status_client.check_tx_id(tx_id);
            if(!res.has_value()) {
                m_logger->error("Timeout waiting for shard response");
                success = false;
            } else {
                if(res.value()) {
                    m_logger->info(to_string(tx_id), "confirmed");
                    confirm_transaction(tx_id);
                } else {
                    m_logger->info(to_string(tx_id), "not found");
                }
            }
        }

        return success;
    }

    auto
    twophase_client::check_tx_id(const hash_t& tx_id) -> std::optional<bool> {
        return m_shard_status_client.check_tx_id(tx_id);
    }

    auto twophase_client::check_unspent(const hash_t& uhs_id)
        -> std::optional<bool> {
        return m_shard_status_client.check_unspent(uhs_id);
    }

    auto twophase_client::send_mint_tx(const transaction::full_tx& mint_tx)
        -> bool {
        auto ctx = transaction::compact_tx(mint_tx);
        for(size_t i = 0; i < m_opts.m_attestation_threshold; i++) {
            auto att
                = ctx.sign(m_secp.get(), m_opts.m_sentinel_private_keys[i]);
            ctx.m_attestations.insert(att);
        }
        auto done = std::promise<void>();
        auto done_fut = done.get_future();
        auto res = m_coordinator_client.execute_transaction(
            ctx,
            [&, tx_id = ctx.m_id](std::optional<bool> success) {
                if(!success.has_value()) {
                    m_logger->error(
                        "Coordinator error processing transaction");
                    return;
                }
                if(!success.value()) {
                    m_logger->error("Coordinator rejected transaction");
                    return;
                }
                confirm_transaction(tx_id);
                m_logger->info("Confirmed mint TX");
                done.set_value();
            });
        if(!res) {
            m_logger->error("Failed to send transaction to coordinator");
            return false;
        }
        constexpr auto timeout = std::chrono::seconds(5);
        auto maybe_timeout = done_fut.wait_for(timeout);
        if(maybe_timeout == std::future_status::timeout) {
            m_logger->error("Timed out waiting for mint response");
            return false;
        }
        return res;
    }
}
