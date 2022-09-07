// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include "uhs/twophase/coordinator/format.hpp"
#include "util/rpc/tcp_server.hpp"
#include "util/serialization/util.hpp"

#include <utility>

namespace cbdc::sentinel_2pc {
    controller::controller(uint32_t sentinel_id,
                           const config::options& opts,
                           std::shared_ptr<logging::log> logger)
        : m_sentinel_id(sentinel_id),
          m_opts(opts),
          m_logger(std::move(logger)),
          m_coordinator_client(
              opts.m_coordinator_endpoints[sentinel_id
                                           % static_cast<uint32_t>(
                                               opts.m_coordinator_endpoints
                                                   .size())]) {}

    auto controller::init() -> bool {
        auto skey = m_opts.m_sentinel_private_keys.find(m_sentinel_id);
        if(skey == m_opts.m_sentinel_private_keys.end()) {
            if(m_opts.m_attestation_threshold > 0) {
                m_logger->error("No private key specified");
                return false;
            }
        } else {
            m_privkey = skey->second;

            auto pubkey = pubkey_from_privkey(m_privkey, m_secp.get());
            m_logger->info("Sentinel public key:", cbdc::to_string(pubkey));
        }

        if(!m_coordinator_client.init()) {
            m_logger->warn("Failed to start coordinator client");
        }

        for(const auto& ep : m_opts.m_sentinel_endpoints) {
            if(ep == m_opts.m_sentinel_endpoints[m_sentinel_id]) {
                continue;
            }
            auto client = std::make_unique<sentinel::rpc::client>(
                std::vector<network::endpoint_t>{ep},
                m_logger);
            if(!client->init()) {
                m_logger->warn("Failed to start sentinel client");
            }
            m_sentinel_clients.emplace_back(std::move(client));
        }

        m_dist = decltype(m_dist)(0, m_sentinel_clients.size() - 1);

        auto rpc_server = std::make_unique<cbdc::rpc::tcp_server<
            cbdc::rpc::async_server<cbdc::sentinel::request,
                                    cbdc::sentinel::response>>>(
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

    auto controller::execute_transaction(
        transaction::full_tx tx,
        execute_result_callback_type result_callback) -> bool {
        const auto validation_err = transaction::validation::check_tx(tx);
        if(validation_err.has_value()) {
            auto tx_id = transaction::tx_id(tx);
            m_logger->debug(
                "Rejected (",
                transaction::validation::to_string(validation_err.value()),
                ")",
                to_string(tx_id));
            result_callback(cbdc::sentinel::execute_response{
                cbdc::sentinel::tx_status::static_invalid,
                validation_err});
            return true;
        }

        auto compact_tx = cbdc::transaction::compact_tx(tx);

        if(m_opts.m_attestation_threshold > 0) {
            auto attestation = compact_tx.sign(m_secp.get(), m_privkey);
            compact_tx.m_attestations.insert(attestation);
        }

        gather_attestations(tx, std::move(result_callback), compact_tx, {});

        return true;
    }

    void
    controller::result_handler(std::optional<bool> res,
                               const execute_result_callback_type& res_cb) {
        if(res.has_value()) {
            auto resp = cbdc::sentinel::execute_response{
                cbdc::sentinel::tx_status::confirmed,
                std::nullopt};
            if(!res.value()) {
                resp.m_tx_status = cbdc::sentinel::tx_status::state_invalid;
            }
            res_cb(resp);
        } else {
            res_cb(std::nullopt);
        }
    }

    auto controller::validate_transaction(
        transaction::full_tx tx,
        validate_result_callback_type result_callback) -> bool {
        const auto validation_err = transaction::validation::check_tx(tx);
        if(validation_err.has_value()) {
            result_callback(std::nullopt);
            return true;
        }
        auto compact_tx = cbdc::transaction::compact_tx(tx);
        auto attestation = compact_tx.sign(m_secp.get(), m_privkey);
        result_callback(std::move(attestation));
        return true;
    }

    void controller::validate_result_handler(
        validate_result v_res,
        const transaction::full_tx& tx,
        execute_result_callback_type result_callback,
        transaction::compact_tx ctx,
        std::unordered_set<size_t> requested) {
        if(!v_res.has_value()) {
            m_logger->error(to_string(ctx.m_id),
                            "invalid according to remote sentinel");
            result_callback(std::nullopt);
            return;
        }
        ctx.m_attestations.insert(std::move(v_res.value()));
        gather_attestations(tx,
                            std::move(result_callback),
                            ctx,
                            std::move(requested));
    }

    void controller::gather_attestations(
        const transaction::full_tx& tx,
        execute_result_callback_type result_callback,
        const transaction::compact_tx& ctx,
        std::unordered_set<size_t> requested) {
        if(ctx.m_attestations.size() < m_opts.m_attestation_threshold) {
            auto success = false;
            while(!success) {
                auto sentinel_id = m_dist(m_rand);
                if(requested.find(sentinel_id) != requested.end()) {
                    continue;
                }
                success
                    = m_sentinel_clients[sentinel_id]->validate_transaction(
                        tx,
                        [=](validate_result v_res) {
                            auto r = requested;
                            r.insert(sentinel_id);
                            validate_result_handler(v_res,
                                                    tx,
                                                    result_callback,
                                                    ctx,
                                                    r);
                        });
            }
            return;
        }

        m_logger->debug("Accepted", to_string(ctx.m_id));

        send_compact_tx(ctx, std::move(result_callback));
    }

    void
    controller::send_compact_tx(const transaction::compact_tx& ctx,
                                execute_result_callback_type result_callback) {
        auto cb =
            [&, res_cb = std::move(result_callback)](std::optional<bool> res) {
                result_handler(res, res_cb);
            };

        // TODO: add a "retry" error response to offload sentinels from this
        //       infinite retry responsibility.
        while(!m_coordinator_client.execute_transaction(ctx, cb)) {
            // TODO: the network currently doesn't provide a callback for
            //       reconnection events so we have to sleep here to
            //       prevent a needless spin. Instead, add such a callback
            //       or queue to the network to remove this sleep.
            static constexpr auto retry_delay = std::chrono::milliseconds(100);
            std::this_thread::sleep_for(retry_delay);
        };
    }
}
