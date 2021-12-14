// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "atomizer_raft.hpp"

#include "format.hpp"
#include "raft/serialization.hpp"
#include "serialization/util.hpp"

namespace cbdc::atomizer {
    atomizer_raft::atomizer_raft(uint32_t atomizer_id,
                                 const network::endpoint_t& raft_endpoint,
                                 size_t stxo_cache_depth,
                                 std::shared_ptr<logging::log> logger,
                                 nuraft::cb_func::func_type raft_callback)
        : node(static_cast<int>(atomizer_id),
               raft_endpoint,
               m_node_type,
               false,
               nuraft::cs_new<state_machine>(
                   stxo_cache_depth,
                   "atomizer_snps_" + std::to_string(atomizer_id)),
               0,
               std::move(logger),
               std::move(raft_callback)) {}

    auto atomizer_raft::get_sm() -> state_machine* {
        auto* cls = dynamic_cast<state_machine*>(node::get_sm());
        assert(cls != nullptr);
        return cls;
    }

    auto atomizer_raft::make_block(const raft::callback_type& result_fn)
        -> bool {
        auto new_log = nuraft::buffer::alloc(sizeof(state_machine::command));
        nuraft::buffer_serializer bs(new_log);

        bs.put_u8(static_cast<uint8_t>(state_machine::command::make_block));

        return replicate(new_log, result_fn);
    }

    auto atomizer_raft::get_block(const cbdc::network::message_t& pkt,
                                  const raft::callback_type& result_fn)
        -> bool {
        const auto log_sz = pkt.m_pkt->size();
        auto new_log = nuraft::buffer::alloc(log_sz);
        nuraft::buffer_serializer bs(new_log);

        bs.put_raw(pkt.m_pkt->data(), pkt.m_pkt->size());

        return replicate(new_log, result_fn);
    }

    void atomizer_raft::prune(const cbdc::network::message_t& pkt) {
        const auto log_sz = pkt.m_pkt->size();
        auto new_log = nuraft::buffer::alloc(log_sz);
        nuraft::buffer_serializer bs(new_log);

        bs.put_raw(pkt.m_pkt->data(), pkt.m_pkt->size());
        [[maybe_unused]] auto res = replicate(new_log, nullptr);
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
            auto agg = aggregate_tx_notify();
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
        auto atns = aggregate_tx_notify_set();
        {
            std::lock_guard<std::mutex> l(m_complete_mut);
            std::swap(atns.m_agg_txs, m_complete_txs);
        }
        if(atns.m_agg_txs.empty()) {
            return false;
        }

        atns.m_cmd = state_machine::command::tx_notify;

        auto new_log = nuraft::buffer::alloc(cbdc::serialized_size(atns));
        auto ser = cbdc::nuraft_serializer(*new_log);
        ser << atns;
        [[maybe_unused]] auto res = replicate(new_log, result_fn);
        return true;
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
