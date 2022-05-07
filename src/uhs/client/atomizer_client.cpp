// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "atomizer_client.hpp"

#include "uhs/atomizer/atomizer/format.hpp"
#include "uhs/atomizer/watchtower/watchtower.hpp"

namespace cbdc {
    atomizer_client::atomizer_client(
        const cbdc::config::options& opts,
        const std::shared_ptr<logging::log>& logger,
        const std::string& wallet_file,
        const std::string& client_file)
        : client(opts, logger, wallet_file, client_file),
          m_wc(opts.m_watchtower_client_endpoints[0]),
          m_logger(logger),
          m_opts(opts) {}

    atomizer_client::~atomizer_client() {
        m_atomizer_network.close();
    }

    auto atomizer_client::init_derived() -> bool {
        m_atomizer_network.cluster_connect(m_opts.m_atomizer_endpoints);
        if(!m_atomizer_network.connected_to_one()) {
            m_logger->warn("Failed to connect to any atomizers");
        }

        if(!m_wc.init()) {
            m_logger->warn("Failed to initialize watchtower client");
        }

        return true;
    }

    auto atomizer_client::sync() -> bool {
        auto tus = cbdc::watchtower::tx_id_uhs_ids();
        for(const auto& [tx_id, tx] : pending_txs()) {
            auto ctx = transaction::compact_tx(tx);
            auto [it, success] = tus.insert({ctx.m_id, {}});
            assert(success);
            it->second.insert(it->second.end(),
                              ctx.m_inputs.begin(),
                              ctx.m_inputs.end());
            std::vector<hash_t> uhs_ids{};
            std::transform(ctx.m_outputs.begin(),
                           ctx.m_outputs.end(),
                           std::back_inserter(uhs_ids),
                           [](transaction::compact_output p) -> hash_t {
                               return p.m_id;
                           });
            it->second.insert(it->second.end(),
                              uhs_ids.begin(),
                              uhs_ids.end());
        }

        for(const auto& [tx_id, in] : pending_inputs()) {
            tus.insert({tx_id, {in.m_prevout_data.m_id}});
        }

        cbdc::watchtower::status_update_request req{tus};

        m_logger->debug("Checking watchtower state...");

        auto res = m_wc.request_status_update(req);

        bool success{true};
        for(const auto& [tx_id, uhs_states] : res->states()) {
            for(const auto& s : uhs_states) {
                if((s.status() != cbdc::watchtower::search_status::unspent)
                   && (s.status() != cbdc::watchtower::search_status::spent)) {
                    m_logger->warn("Tx ID:",
                                   to_string(tx_id),
                                   ", UHS ID:",
                                   to_string(s.uhs_id()),
                                   ", status:",
                                   static_cast<uint32_t>(s.status()));
                    success = false;
                    continue;
                }
                break;
            }
            if(!success) {
                continue;
            }
            success = confirm_transaction(tx_id);
        }

        return success;
    }

    auto atomizer_client::send_mint_tx(const transaction::full_tx& mint_tx)
        -> bool {
        atomizer::tx_notify_request msg;
        auto ctx = transaction::compact_tx(mint_tx);
        for(size_t i = 0; i < m_opts.m_attestation_threshold; i++) {
            auto att
                = ctx.sign(m_secp.get(), m_opts.m_sentinel_private_keys[i]);
            ctx.m_attestations.insert(att);
        }
        msg.m_tx = std::move(ctx);
        msg.m_block_height = m_wc.request_best_block_height()->height();
        return m_atomizer_network.send_to_one(atomizer::request{msg});
    }
}
