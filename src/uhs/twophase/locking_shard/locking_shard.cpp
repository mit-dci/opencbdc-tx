// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "locking_shard.hpp"

#include "format.hpp"
#include "messages.hpp"
#include "uhs/transaction/validation.hpp"
#include "util/common/config.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/istream_serializer.hpp"

namespace cbdc::locking_shard {
    auto locking_shard::discard_dtx(const hash_t& dtx_id) -> bool {
        std::unique_lock<std::shared_mutex> l(m_mut);
        bool running = m_running;
        if(running) {
            m_applied_dtxs.erase(dtx_id);
        }
        return running;
    }

    locking_shard::locking_shard(
        const std::pair<uint8_t, uint8_t>& output_range,
        std::shared_ptr<logging::log> logger,
        size_t completed_txs_cache_size,
        const std::string& preseed_file,
        config::options opts)
        : interface(output_range),
          m_logger(std::move(logger)),
          m_completed_txs(completed_txs_cache_size),
          m_opts(std::move(opts)) {
        m_uhs.max_load_factor(std::numeric_limits<float>::max());
        m_applied_dtxs.max_load_factor(std::numeric_limits<float>::max());
        m_prepared_dtxs.max_load_factor(std::numeric_limits<float>::max());
        m_locked.max_load_factor(std::numeric_limits<float>::max());

        static constexpr auto dtx_buckets = 100000;
        m_applied_dtxs.rehash(dtx_buckets);
        m_prepared_dtxs.rehash(dtx_buckets);

        static constexpr auto locked_buckets = 10000000;
        m_locked.rehash(locked_buckets);

        if(!preseed_file.empty()) {
            m_logger->info("Reading preseed file into memory");
            if(!read_preseed_file(preseed_file)) {
                m_logger->error("Preseeding failed");
            } else {
                m_logger->info("Preseeding complete -", m_uhs.size(), "utxos");
            }
        }
    }

    auto locking_shard::read_preseed_file(const std::string& preseed_file)
        -> bool {
        if(std::filesystem::exists(preseed_file)) {
            auto in = std::ifstream(preseed_file, std::ios::binary);
            in.seekg(0, std::ios::end);
            auto sz = in.tellg();
            if(sz == -1) {
                return false;
            }
            in.seekg(0, std::ios::beg);
            auto deser = istream_serializer(in);
            m_uhs.clear();
            static constexpr auto uhs_size_factor = 2;
            auto bucket_count = static_cast<unsigned long>(sz / cbdc::hash_size
                                                           * uhs_size_factor);
            m_uhs.rehash(bucket_count);
            deser >> m_uhs;
            return true;
        }
        return false;
    }

    auto locking_shard::lock_outputs(std::vector<tx>&& txs,
                                     const hash_t& dtx_id)
        -> std::optional<std::vector<bool>> {
        std::unique_lock<std::shared_mutex> l(m_mut);
        if(!m_running) {
            return std::nullopt;
        }

        auto prepared_dtx_it = m_prepared_dtxs.find(dtx_id);
        if(prepared_dtx_it != m_prepared_dtxs.end()) {
            return prepared_dtx_it->second.m_results;
        }

        auto ret = std::vector<bool>();
        ret.reserve(txs.size());
        for(auto&& tx : txs) {
            auto success = check_and_lock_tx(tx);
            ret.push_back(success);
            m_highest_epoch = std::max(m_highest_epoch, tx.m_epoch);
        }
        auto p = prepared_dtx();
        p.m_results = ret;
        p.m_txs = std::move(txs);
        m_prepared_dtxs.emplace(dtx_id, std::move(p));
        return ret;
    }

    auto locking_shard::check_and_lock_tx(const tx& t) -> bool {
        bool success{true};
        if(!transaction::validation::check_attestations(
               t.m_tx,
               m_opts.m_sentinel_public_keys,
               m_opts.m_attestation_threshold)) {
            m_logger->warn("Received invalid compact transaction",
                           to_string(t.m_tx.m_id));
            success = false;
        }
        if(success) {
            for(const auto& uhs_id : t.m_tx.m_inputs) {
                if(hash_in_shard_range(uhs_id.m_id)
                   && m_uhs.find(uhs_id.m_id) == m_uhs.end()) {
                    success = false;
                    break;
                }
            }
        }
        if(success) {
            for(const auto& uhs_id : t.m_tx.m_inputs) {
                if(hash_in_shard_range(uhs_id.m_id)) {
                    auto it = m_uhs.find(uhs_id.m_id);
                    assert(it != m_uhs.end());
                    m_locked.emplace(uhs_id.m_id, it->second);
                    m_uhs.erase(uhs_id.m_id);
                }
            }
        }
        return success;
    }

    auto locking_shard::apply_outputs(std::vector<bool>&& complete_txs,
                                      const hash_t& dtx_id) -> bool {
        std::unique_lock<std::shared_mutex> l(m_mut);
        if(!m_running) {
            return false;
        }
        auto prepared_dtx_it = m_prepared_dtxs.find(dtx_id);
        if(prepared_dtx_it == m_prepared_dtxs.end()) {
            if(m_applied_dtxs.find(dtx_id) == m_applied_dtxs.end()) {
                m_logger->fatal("Unable to find dtx data for apply",
                                to_string(dtx_id));
            }
            return true;
        }
        auto& dtx = prepared_dtx_it->second.m_txs;
        if(complete_txs.size() != dtx.size()) {
            // This would only happen due to a bug in the controller
            m_logger->fatal("Incorrect number of complete tx flags for apply",
                            to_string(dtx_id),
                            complete_txs.size(),
                            "vs",
                            dtx.size());
        }
        for(size_t i{0}; i < dtx.size(); i++) {
            auto&& tx = dtx[i];
            apply_tx(tx, complete_txs[i]);
        }

        m_prepared_dtxs.erase(dtx_id);
        m_applied_dtxs.insert(dtx_id);
        return true;
    }

    void locking_shard::stop() {
        m_running = false;
    }

    auto locking_shard::check_unspent(const hash_t& uhs_id)
        -> std::optional<bool> {
        std::shared_lock<std::shared_mutex> l(m_mut);
        return m_uhs.find(uhs_id) != m_uhs.end()
            || m_locked.find(uhs_id) != m_locked.end();
    }

    auto locking_shard::check_tx_id(const hash_t& tx_id)
        -> std::optional<bool> {
        return m_completed_txs.contains(tx_id);
    }

    void locking_shard::apply_tx(const tx& t, bool complete) {
        if(hash_in_shard_range(t.m_tx.m_id)) {
            m_completed_txs.add(t.m_tx.m_id);
        }
        for(auto&& uhs_id : t.m_tx.m_uhs_outputs) {
            if(!(hash_in_shard_range(uhs_id.m_id) && complete)) {
                continue;
            }
            m_uhs.emplace(
                uhs_id.m_id,
                uhs_element{uhs_id.m_data, uhs_id.m_value, t.m_epoch});
        }
        for(auto&& uhs_id : t.m_tx.m_inputs) {
            if(!hash_in_shard_range(uhs_id.m_id)) {
                continue;
            }
            auto it = m_locked.find(uhs_id.m_id);
            if(it == m_locked.end()) {
                continue;
            }
            auto elem = it->second;
            if(!complete) {
                m_uhs.emplace(uhs_id.m_id, elem);
            } else {
                elem.m_deletion_epoch = t.m_epoch;
                m_spent.emplace(uhs_id.m_id, elem);
            }
            m_locked.erase(uhs_id.m_id);
        }
    }

    auto locking_shard::audit(uint64_t epoch) const
        -> std::optional<uint64_t> {
        auto uhs = decltype(m_uhs)();
        auto locked = decltype(m_locked)();
        auto spent = decltype(m_spent)();
        {
            std::shared_lock l(m_mut);
            uhs = m_uhs;
            locked = m_locked;
            spent = m_spent;
        }

        auto check_uhs
            = [epoch](
                  const std::unordered_map<hash_t, uhs_element, hashing::null>&
                      uhs_map) -> std::optional<uint64_t> {
            uint64_t tot{};
            for(const auto& [id, elem] : uhs_map) {
                if(elem.m_creation_epoch <= epoch
                   && (!elem.m_deletion_epoch.has_value()
                       || (elem.m_deletion_epoch.value() > epoch))) {
                    auto uhs_id = transaction::calculate_uhs_id(elem.m_data,
                                                                elem.m_value);
                    if(uhs_id != id) {
                        return std::nullopt;
                    }
                    tot += elem.m_value;
                }
            }
            return tot;
        };

        uint64_t tot{};
        auto uhs_tot = check_uhs(uhs);
        if(!uhs_tot.has_value()) {
            return std::nullopt;
        }
        tot += uhs_tot.value();

        auto locked_tot = check_uhs(locked);
        if(!locked_tot.has_value()) {
            return std::nullopt;
        }
        tot += locked_tot.value();

        auto spent_tot = check_uhs(spent);
        if(!spent_tot.has_value()) {
            return std::nullopt;
        }
        tot += spent_tot.value();

        return tot;
    }

    auto locking_shard::highest_epoch() const -> uint64_t {
        std::shared_lock l(m_mut);
        return m_highest_epoch;
    }

    void locking_shard::prune(uint64_t epoch) {
        std::unique_lock l(m_mut);
        for(auto it = m_spent.begin(); it != m_spent.end();) {
            auto& elem = it->second;
            if(elem.m_deletion_epoch.value() < epoch) {
                it = m_spent.erase(it);
            } else {
                it++;
            }
        }
    }
}
