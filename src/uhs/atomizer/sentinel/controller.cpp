// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include "uhs/sentinel/format.hpp"
#include "util/rpc/tcp_server.hpp"

#include <random>
#include <utility>

namespace cbdc::sentinel {
    controller::controller(uint32_t sentinel_id,
                           config::options opts,
                           std::shared_ptr<logging::log> logger)
        : m_sentinel_id(sentinel_id),
          m_opts(std::move(opts)),
          m_logger(std::move(logger)) {}

    auto controller::init() -> bool {
        auto skey = m_opts.m_sentinel_private_keys.find(m_sentinel_id);
        if(skey == m_opts.m_sentinel_private_keys.end()) {
            m_logger->error("No private key specified");
            return false;
        }
        m_privkey = skey->second;

        auto pubkey = pubkey_from_privkey(m_privkey, m_secp.get());
        m_logger->info("Sentinel public key:", cbdc::to_string(pubkey));

        m_shard_data.reserve(m_opts.m_shard_endpoints.size());
        for(size_t i{0}; i < m_opts.m_shard_endpoints.size(); i++) {
            const auto& shard = m_opts.m_shard_endpoints[i];
            m_logger->info("Connecting to",
                           shard.first,
                           ":",
                           shard.second,
                           "...");
            auto sock = std::make_unique<network::tcp_socket>();
            if(!sock->connect(shard)) {
                m_logger->warn("failed to connect");
            }

            const auto peer_id = m_shard_network.add(std::move(sock));
            const auto& shard_range = m_opts.m_shard_ranges[i];
            m_shard_data.push_back(shard_info{shard_range, peer_id});

            m_logger->info("done");
        }

        m_shard_dist = decltype(m_shard_dist)(0, m_shard_data.size() - 1);

        for(const auto& ep : m_opts.m_sentinel_endpoints) {
            if(ep == m_opts.m_sentinel_endpoints[m_sentinel_id]) {
                continue;
            }
            auto client = std::make_unique<sentinel::rpc::client>(
                std::vector<network::endpoint_t>{ep},
                m_logger);
            if(!client->init()) {
                m_logger->error("Failed to start sentinel client");
                return false;
            }
            m_sentinel_clients.emplace_back(std::move(client));
        }

        m_dist = decltype(m_dist)(0, m_sentinel_clients.size() - 1);

        auto rpc_server = std::make_unique<
            cbdc::rpc::tcp_server<cbdc::rpc::async_server<request, response>>>(
            m_opts.m_sentinel_endpoints[m_sentinel_id]);
        if(!rpc_server->init()) {
            m_logger->error("Failed to start sentinel RPC server");
            return false;
        }

        m_rpc_server = std::make_unique<decltype(m_rpc_server)::element_type>(
            this,
            std::move(rpc_server));

        return true;
    }

    auto controller::execute_transaction(transaction::full_tx tx)
        -> std::optional<cbdc::sentinel::execute_response> {
        const auto res = transaction::validation::check_tx(tx);
        tx_status status{tx_status::pending};
        if(res.has_value()) {
            status = tx_status::static_invalid;
        }

        auto tx_id = transaction::tx_id(tx);

        if(!res.has_value()) {
            m_logger->debug("Accepted tx:", cbdc::to_string(tx_id));
            // Only forward transactions that are valid
            send_transaction(tx);
        } else {
            m_logger->debug("Rejected tx:",
                            cbdc::to_string(tx_id),
                            "(",
                            cbdc::transaction::validation::to_string(res.value()),
                            ")");
        }

        return execute_response{status, res};
    }

    // todo: need to take a full_tx instead of a compact_tx for
    // gather_attestations
    void controller::send_transaction(const transaction::full_tx& tx) {
        auto compact_tx = cbdc::transaction::compact_tx(tx);
        auto attestation = compact_tx.sign(m_secp.get(), m_privkey);
        compact_tx.m_attestations.insert(attestation);
        auto ctx_pkt = std::make_shared<cbdc::buffer>();
        auto ctx_ser = cbdc::buffer_serializer(*ctx_pkt);
        ctx_ser << compact_tx;

        gather_attestations(tx, compact_tx, {});
    }

    auto controller::validate_transaction(transaction::full_tx tx)
        -> std::optional<validate_response> {
        const auto res = transaction::validation::check_tx(tx);
        if(res.has_value()) {
            return std::nullopt;
        }
        auto compact_tx = cbdc::transaction::compact_tx(tx);
        auto attestation = compact_tx.sign(m_secp.get(), m_privkey);
        return attestation;
    }

    void
    controller::validate_result_handler(async_interface::validate_result v_res,
                                        const transaction::full_tx& tx,
                                        transaction::compact_tx ctx,
                                        std::unordered_set<size_t> requested) {
        if(!v_res.has_value()) {
            m_logger->error(cbdc::to_string(ctx.m_id),
                            "invalid according to remote sentinel");
            return;
        }
        ctx.m_attestations.insert(std::move(v_res.value()));
        gather_attestations(tx, ctx, std::move(requested));
    }

    void
    controller::gather_attestations(const transaction::full_tx& tx,
                                    const transaction::compact_tx& ctx,
                                    std::unordered_set<size_t> requested) {
        if(ctx.m_attestations.size() < m_opts.m_attestation_threshold) {
            auto success = false;
            while(!success) {
                auto sentinel_id = [&]() {
                    std::unique_lock l(m_rand_mut);
                    return m_dist(m_rand);
                }();
                if(requested.find(sentinel_id) != requested.end()) {
                    continue;
                }
                success
                    = m_sentinel_clients[sentinel_id]->validate_transaction(
                        tx,
                        [=](async_interface::validate_result v_res) {
                            auto r = requested;
                            r.insert(sentinel_id);
                            validate_result_handler(v_res, tx, ctx, r);
                        });
            }
            return;
        }

        send_compact_tx(ctx);
    }

    void controller::send_compact_tx(const transaction::compact_tx& ctx) {
        auto ctx_pkt = std::make_shared<cbdc::buffer>(cbdc::make_buffer(ctx));

        auto offset = [&]() {
            std::unique_lock l(m_rand_mut);
            return m_shard_dist(m_rand);
        }();
        auto inputs_sent = std::unordered_set<size_t>();
        for(size_t i = 0; i < m_shard_data.size(); i++) {
            auto idx = (i + offset) % m_shard_data.size();
            const auto& range = m_shard_data[idx].m_range;
            const auto& pid = m_shard_data[idx].m_peer_id;
            if(inputs_sent.size() == ctx.m_inputs.size()) {
                break;
            }
            if(!m_shard_network.connected(pid)) {
                continue;
            }
            auto should_send = false;
            for(size_t j = 0; j < ctx.m_inputs.size(); j++) {
                if(inputs_sent.find(j) != inputs_sent.end()) {
                    continue;
                }
                if(!config::hash_in_shard_range(range, ctx.m_inputs[i])) {
                    continue;
                }
                inputs_sent.insert(j);
                should_send = true;
            }
            if(should_send) {
                m_shard_network.send(ctx_pkt, pid);
            }
        }
    }
}
