// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "block_cache.hpp"

namespace cbdc::watchtower {
    block_cache::block_cache(size_t k) : m_k_blks(k) {
        static constexpr auto puts_per_tx = 2;
        static constexpr auto txs_per_block = 1000000;
        m_spent_ids.reserve(k * txs_per_block * puts_per_tx);
        m_unspent_ids.reserve(k * txs_per_block * puts_per_tx);
    }

    void block_cache::push_block(cbdc::atomizer::block&& blk) {
        if((m_k_blks != 0) && (m_blks.size() == m_k_blks)) {
            auto& old_blk = m_blks.front();
            for(auto& tx : old_blk.m_transactions) {
                for(auto& in : tx.m_inputs) {
                    m_spent_ids.erase(in);
                }
                for(auto& out : tx.m_outputs) {
                    m_unspent_ids.erase(out.m_id);
                }
            }
            m_blks.pop();
        }

        m_blks.push(std::forward<cbdc::atomizer::block>(blk));

        auto blk_height = m_blks.back().m_height;
        for(auto& tx : m_blks.back().m_transactions) {
            for(auto& in : tx.m_inputs) {
                m_unspent_ids.erase(in);
                m_spent_ids.insert(
                    {{in, std::make_pair(blk_height, tx.m_id)}});
            }
            for(auto& out : tx.m_outputs) {
                m_unspent_ids.insert(
                    {{out.m_id, std::make_pair(blk_height, tx.m_id)}});
            }
        }
        m_best_blk_height = std::max(m_best_blk_height, blk_height);
    }

    auto block_cache::check_unspent(const hash_t& uhs_id) const
        -> std::optional<block_cache_result> {
        auto res = m_unspent_ids.find(uhs_id);
        if(res == m_unspent_ids.end()) {
            return std::nullopt;
        }
        return res->second;
    }

    auto block_cache::check_spent(const hash_t& uhs_id) const
        -> std::optional<block_cache_result> {
        auto res = m_spent_ids.find(uhs_id);
        if(res == m_spent_ids.end()) {
            return std::nullopt;
        }
        return res->second;
    }
    auto block_cache::best_block_height() const -> uint64_t {
        return m_best_blk_height;
    }
}
