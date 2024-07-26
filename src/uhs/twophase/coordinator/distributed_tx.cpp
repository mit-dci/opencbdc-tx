// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "distributed_tx.hpp"

#include <future>

namespace cbdc::coordinator {
    distributed_tx::distributed_tx(
        const hash_t& dtx_id,
        std::vector<std::shared_ptr<locking_shard::interface>> shards,
        std::shared_ptr<logging::log> logger)
        : m_dtx_id(dtx_id),
          m_shards(std::move(shards)),
          m_logger(std::move(logger)) {
        m_txs.resize(m_shards.size());
        m_tx_idxs.resize(m_shards.size());
        assert(!m_shards.empty());
    }

    auto distributed_tx::prepare() -> std::optional<std::vector<bool>> {
        if(m_prepare_cb) {
            auto res = m_prepare_cb(m_dtx_id, m_full_txs);
            if(!res) {
                m_state = dtx_state::failed;
                return std::nullopt;
            }
        }
        auto futures = std::vector<
            std::pair<std::future<std::optional<std::vector<bool>>>,
                      size_t>>();
        for(size_t i{0}; i < m_shards.size(); i++) {
            if(m_tx_idxs[i].empty()) {
                continue;
            }
            const auto& shard = m_shards[i];
            auto f = std::async(std::launch::async,
                                &locking_shard::interface::lock_outputs,
                                shard,
                                std::move(m_txs[i]),
                                m_dtx_id);
            futures.emplace_back(std::move(f), i);
        }
        auto ret = std::vector<bool>(m_full_txs.size(), true);
        for(auto& f : futures) {
            auto res = f.first.get();
            if(!res) {
                m_state = dtx_state::failed;
                return std::nullopt;
            }
            if(res->size() != m_tx_idxs[f.second].size()) {
                m_logger->fatal(
                    "Shard prepare response has not enough statuses",
                    to_string(m_dtx_id),
                    "expected:",
                    m_tx_idxs[f.second].size(),
                    "got:",
                    res->size());
            }
            for(size_t i{0}; i < res->size(); i++) {
                if(!(*res)[i]) {
                    ret[m_tx_idxs[f.second][i]] = false;
                }
            }
        }
        m_state = dtx_state::commit;
        return ret;
    }

    auto
    distributed_tx::commit(const std::vector<bool>& complete_txs) -> bool {
        if(m_commit_cb) {
            auto res = m_commit_cb(m_dtx_id, complete_txs, m_tx_idxs);
            if(!res) {
                m_state = dtx_state::failed;
                return false;
            }
        }
        auto futures = std::vector<std::future<bool>>();
        for(size_t i{0}; i < m_shards.size(); i++) {
            if(m_tx_idxs[i].empty()) {
                continue;
            }
            const auto& shard = m_shards[i];
            auto shard_complete_txs = std::vector<bool>(m_tx_idxs[i].size());
            for(size_t j{0}; j < shard_complete_txs.size(); j++) {
                shard_complete_txs[j] = complete_txs[m_tx_idxs[i][j]];
            }
            auto f = std::async(std::launch::async,
                                &locking_shard::interface::apply_outputs,
                                shard,
                                std::move(shard_complete_txs),
                                m_dtx_id);
            futures.emplace_back(std::move(f));
        }
        for(auto& f : futures) {
            auto res = f.get();
            if(!res) {
                m_state = dtx_state::failed;
                return false;
            }
        }
        m_state = dtx_state::discard;
        return true;
    }

    auto distributed_tx::execute() -> std::optional<std::vector<bool>> {
        auto dtxid_str = to_string(m_dtx_id);
        if(m_state == dtx_state::prepare || m_state == dtx_state::start) {
            m_logger->info("Preparing", dtxid_str);
            auto res = prepare();
            if(!res) {
                return std::nullopt;
            }
            m_complete_txs = std::move(*res);
            if(m_complete_txs.size() != m_full_txs.size()) {
                m_logger->fatal("Prepare has incorrect number of statuses",
                                dtxid_str,
                                "expected:",
                                m_full_txs.size(),
                                "got:",
                                m_complete_txs.size());
            }
            m_logger->info("Prepared", dtxid_str);
        }
        if(m_state == dtx_state::commit) {
            m_logger->info("Committing", dtxid_str);
            auto res = commit(m_complete_txs);
            if(!res) {
                return std::nullopt;
            }
            m_logger->info("Committed", dtxid_str);
        }
        if(m_state == dtx_state::discard) {
            m_logger->info("Discarding", dtxid_str);
            auto res = discard();
            if(!res) {
                return std::nullopt;
            }
            m_logger->info("Discarded", dtxid_str);
        }
        return m_complete_txs;
    }

    auto distributed_tx::add_tx(const transaction::compact_tx& tx) -> size_t {
        for(size_t i{0}; i < m_shards.size(); i++) {
            const auto& shard = m_shards[i];
            auto stx = locking_shard::tx();
            stx.m_tx = tx;
            bool active{false};
            if(shard->hash_in_shard_range(tx.m_id)) {
                active = true;
            } else {
                for(const auto& inp : tx.m_inputs) {
                    if(shard->hash_in_shard_range(inp)) {
                        active = true;
                        break;
                    }
                }
                if(!active) {
                    for(const auto& out : tx.m_uhs_outputs) {
                        if(shard->hash_in_shard_range(out)) {
                            active = true;
                            break;
                        }
                    }
                }
            }
            if(active) {
                m_txs[i].emplace_back(std::move(stx));
                m_tx_idxs[i].emplace_back(m_full_txs.size());
            }
        }
        m_full_txs.emplace_back(tx);
        return m_full_txs.size() - 1;
    }

    auto distributed_tx::discard() -> bool {
        if(m_discard_cb) {
            auto res = m_discard_cb(m_dtx_id);
            if(!res) {
                m_state = dtx_state::failed;
                return false;
            }
        }
        auto futures = std::vector<std::future<bool>>();
        for(size_t i{0}; i < m_shards.size(); i++) {
            if(m_tx_idxs[i].empty()) {
                continue;
            }
            const auto& shard = m_shards[i];
            auto f = std::async(std::launch::async,
                                &locking_shard::interface::discard_dtx,
                                shard,
                                m_dtx_id);
            futures.emplace_back(std::move(f));
        }
        for(auto& f : futures) {
            auto res = f.get();
            if(!res) {
                m_state = dtx_state::failed;
                return false;
            }
        }
        if(m_done_cb) {
            auto res = m_done_cb(m_dtx_id);
            if(!res) {
                m_state = dtx_state::failed;
                return false;
            }
        }
        m_state = dtx_state::done;
        return true;
    }

    auto distributed_tx::get_id() const -> hash_t {
        return m_dtx_id;
    }

    void distributed_tx::set_prepare_cb(const prepare_cb_t& cb) {
        m_prepare_cb = cb;
    }

    void distributed_tx::set_commit_cb(const commit_cb_t& cb) {
        m_commit_cb = cb;
    }

    void distributed_tx::set_discard_cb(const discard_cb_t& cb) {
        m_discard_cb = cb;
    }

    void distributed_tx::set_done_cb(const done_cb_t& cb) {
        m_done_cb = cb;
    }

    void distributed_tx::recover_prepare(
        const std::vector<transaction::compact_tx>& txs) {
        m_state = dtx_state::prepare;
        for(const auto& tx : txs) {
            add_tx(tx);
        }
    }

    void distributed_tx::recover_commit(
        const std::vector<bool>& complete_txs,
        const std::vector<std::vector<uint64_t>>& tx_idxs) {
        m_state = dtx_state::commit;
        m_tx_idxs = tx_idxs;
        m_complete_txs = complete_txs;
    }

    void distributed_tx::recover_discard() {
        m_state = dtx_state::discard;
    }

    auto distributed_tx::size() const -> size_t {
        return std::max(m_full_txs.size(), m_complete_txs.size());
    }

    auto distributed_tx::get_state() const -> dtx_state {
        return m_state;
    }
}
