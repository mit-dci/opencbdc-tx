// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "atomizer_raft.hpp"

#include "format.hpp"
#include "util/raft/serialization.hpp"
#include "util/raft/util.hpp"
#include "util/serialization/util.hpp"

namespace cbdc::atomizer {
    atomizer_raft::atomizer_raft(uint32_t atomizer_id,
                                 const network::endpoint_t& raft_endpoint,
                                 size_t stxo_cache_depth,
                                 std::shared_ptr<logging::log> logger,
                                 nuraft::cb_func::func_type raft_callback,
                                 bool wait_for_followers)
        : node(static_cast<int>(atomizer_id),
               raft_endpoint,
               m_node_type,
               false,
               nuraft::cs_new<state_machine>(
                   stxo_cache_depth,
                   "atomizer_snps_" + std::to_string(atomizer_id)),
               0,
               std::move(logger),
               std::move(raft_callback),
               wait_for_followers) {}

    auto atomizer_raft::get_sm() -> state_machine* {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* cls = reinterpret_cast<state_machine*>(node::get_sm());
        assert(cls != nullptr);
        return cls;
    }

    auto atomizer_raft::make_request(const state_machine::request& r,
                                     const raft::callback_type& result_fn)
        -> bool {
        auto new_log
            = make_buffer<state_machine::request, nuraft::ptr<nuraft::buffer>>(
                r);
        return replicate(new_log, result_fn);
    }

    auto atomizer_raft::tx_notify_count() -> uint64_t {
        return get_sm()->tx_notify_count();
    }

    void atomizer_raft::tx_notify(tx_notify_request&& notif) {
        auto it = m_txs.find(notif.m_tx);
        if(it != m_txs.end()) {
            for(auto n : notif.m_attestations) {
                auto p = std::make_pair(n, notif.m_block_height);
                auto n_it = it->second.find(p);
                if((n_it != it->second.end()
                    && n_it->second < notif.m_block_height)
                   || n_it == it->second.end()) {
                    it->second.insert(std::move(p));
                }
            }
        } else {
            auto attestations = attestation_set();
            attestations.reserve(notif.m_attestations.size());
            for(auto n : notif.m_attestations) {
                attestations.insert(std::make_pair(n, notif.m_block_height));
            }
            it = m_txs
                     .insert(std::make_pair(std::move(notif.m_tx),
                                            std::move(attestations)))
                     .first;
        }

        // TODO: handle notifications that never spill over due to lack of
        //       attestations
        if(it->second.size() == it->first.m_inputs.size()) {
            auto agg = aggregate_tx_notification();
            auto tx = m_txs.extract(it);
            agg.m_tx = std::move(tx.key());
            uint64_t oldest{0};
            for(const auto& att : tx.mapped()) {
                if(oldest == 0 || att.second < oldest) {
                    oldest = att.second;
                }
            }
            agg.m_oldest_attestation = oldest;
            {
                std::lock_guard<std::mutex> l(m_complete_mut);
                m_complete_txs.push_back(std::move(agg));
            }
        }
    }

    auto atomizer_raft::send_complete_txs(const raft::callback_type& result_fn)
        -> bool {
        auto atns = aggregate_tx_notify_request();
        {
            std::lock_guard<std::mutex> l(m_complete_mut);
            std::swap(atns.m_agg_txs, m_complete_txs);
        }
        if(atns.m_agg_txs.empty()) {
            return false;
        }
        return make_request(atns, result_fn);
    }

    auto atomizer_raft::attestation_hash::operator()(
        const atomizer_raft::attestation& pair) const -> size_t {
        return std::hash<decltype(pair.first)>()(pair.first);
    }

    auto atomizer_raft::attestation_cmp::operator()(
        const atomizer_raft::attestation& a,
        const atomizer_raft::attestation& b) const -> bool {
        return a.first == b.first;
    }
}
