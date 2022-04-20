// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include "uhs/sentinel/format.hpp"
#include "util/rpc/tcp_server.hpp"
#include "util/serialization/util2.hpp"

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
                m_logger->error("failed to connect");
                return false;
            }

            const auto peer_id = m_shard_network.add(std::move(sock));
            const auto& shard_range = m_opts.m_shard_ranges[i];
            m_shard_data.push_back(shard_info{shard_range, peer_id});

            m_logger->info("done");
        }

        auto rng = std::default_random_engine();
        rng.seed(m_sentinel_id);

        // Shuffle the shards list to spread load between shards
        std::shuffle(m_shard_data.begin(), m_shard_data.end(), rng);

        auto rpc_server = std::make_unique<cbdc::rpc::tcp_server<
            cbdc::rpc::blocking_server<request, response>>>(
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
        -> std::optional<cbdc::sentinel::response> {
        const auto res = transaction::validation::check_tx(tx);
        tx_status status{tx_status::pending};
        if(res.has_value()) {
            status = tx_status::static_invalid;
        }

        auto tx_id = transaction::tx_id(tx);

        if(!res.has_value()) {
            m_logger->debug("Accepted tx:", cbdc::to_string(tx_id));
        } else {
            m_logger->debug("Rejected tx:", cbdc::to_string(tx_id));
        }

        // Only forward transactions that are valid
        if(!res.has_value()) {
            send_transaction(tx);
        }

        return response{status, res};
    }

    void controller::send_transaction(const transaction::full_tx& tx) {
        const auto compact_tx = cbdc::transaction::compact_tx(tx);
        auto ctx_pkt = std::make_shared<cbdc::buffer>();
        auto ctx_ser = cbdc::buffer_serializer(*ctx_pkt);
        ctx_ser << compact_tx;

        auto inputs_sent = std::unordered_set<size_t>();
        for(const auto& [range, pid] : m_shard_data) {
            if(inputs_sent.size() == compact_tx.m_inputs.size()) {
                break;
            }
            if(!m_shard_network.connected(pid)) {
                continue;
            }
            auto should_send = false;
            for(size_t i = 0; i < compact_tx.m_inputs.size(); i++) {
                if(inputs_sent.find(i) != inputs_sent.end()) {
                    continue;
                }
                if(!config::hash_in_shard_range(range,
                                                compact_tx.m_inputs[i])) {
                    continue;
                }
                inputs_sent.insert(i);
                should_send = true;
            }
            if(should_send) {
                m_shard_network.send(ctx_pkt, pid);
            }
        }
    }
}
