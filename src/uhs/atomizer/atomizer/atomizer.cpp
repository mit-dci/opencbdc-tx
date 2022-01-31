// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "atomizer.hpp"

#include "uhs/transaction/messages.hpp"
#include "util/common/config.hpp"
#include "util/serialization/buffer_serializer.hpp"
#include "util/serialization/format.hpp"

namespace cbdc::atomizer {
    auto atomizer::make_block()
        -> std::pair<block, std::vector<cbdc::watchtower::tx_error>> {
        block blk;

        blk.m_transactions.swap(m_complete_txs);

        m_best_height++;

        std::vector<cbdc::watchtower::tx_error> errs;
        for(auto&& tx : m_txs[m_spent_cache_depth]) {
            errs.push_back(cbdc::watchtower::tx_error{
                tx.first.m_id,
                cbdc::watchtower::tx_error_incomplete{}});
        }

        for(size_t i = m_spent_cache_depth; i > 0; i--) {
            m_spent[i] = std::move(m_spent[i - 1]);
            m_txs[i] = std::move(m_txs[i - 1]);
        }

        m_spent[0].clear();
        m_txs[0].clear();
        static constexpr auto initial_spent_cache_size = 500000;
        m_spent[0].reserve(initial_spent_cache_size);

        blk.m_height = m_best_height;

        return {blk, errs};
    }

    auto atomizer::insert(const uint64_t block_height,
                          transaction::compact_tx tx,
                          std::unordered_set<uint32_t> attestations)
        -> std::optional<cbdc::watchtower::tx_error> {
        const auto height_offset = get_notification_offset(block_height);

        auto offset_err = check_notification_offset(height_offset, tx);
        if(offset_err) {
            return offset_err;
        }

        // Search the incomplete transactions vector for this notification's
        // block height offset. Note, we might be able to defer this insertion
        // until after we've checked if the transaction is complete.
        auto it = m_txs[height_offset].find(tx);
        if(it == m_txs[height_offset].end()) {
            // If we did not already receive a notification of this transaction
            // for its height offset, insert the transaction and its
            // attestations into the pending vector.
            it = m_txs[height_offset]
                     .insert({std::move(tx), std::move(attestations)})
                     .first;
        } else {
            // Otherwise merge the new set of attestations with the existing
            // set.
            it->second.insert(attestations.begin(), attestations.end());
        }

        std::unordered_set<uint32_t> total_attestations;
        size_t oldest_attestation{0};
        std::map<size_t, decltype(m_txs)::value_type::const_iterator> tx_its;

        // Iterate over each height offset in the incomplete transactions
        // vector to accumulate the sets of attestations received for any
        // offset in our cache.
        for(size_t offset = 0; offset <= m_spent_cache_depth; offset++) {
            const auto& tx_map = m_txs[offset];

            // Check if we received a notification of this TX for the given
            // height offset.
            const auto tx_it = tx_map.find(it->first);
            if(tx_it != tx_map.end()) {
                // Merge the attestations from this offset with the full set of
                // attestations so far.
                total_attestations.insert(tx_it->second.begin(),
                                          tx_it->second.end());

                // Keep track of the oldest height offset that we're using
                // an attestation from.
                oldest_attestation = offset;

                // Store the iterator to the transaction in the incomplete TXs
                // vector for the given height offset so we can quickly access
                // it later.
                tx_its.emplace(offset, tx_it);
            }
        }

        const auto& txit = it->first;

        auto cache_check_range = oldest_attestation;

        // Check whether this transaction now has attestations for each of its
        // inputs.
        if(total_attestations.size() == txit.m_inputs.size()) {
            auto err_set = check_stxo_cache(txit, cache_check_range);
            if(err_set) {
                return err_set;
            }

            add_tx_to_stxo_cache(txit);

            // For each of the incomplete transaction notifications we
            // recovered while accumulating attestations, either extract the TX
            // from the oldest notification and move it to the complete TXs
            // vector, or erase the TX notification.
            for(const auto& pending_offset : tx_its) {
                if(pending_offset.first == oldest_attestation) {
                    auto tx_ext = m_txs[pending_offset.first].extract(
                        pending_offset.second);
                    m_complete_txs.push_back(std::move(tx_ext.key()));
                } else {
                    m_txs[pending_offset.first].erase(pending_offset.second);
                }
            }
        }

        return std::nullopt;
    }

    auto atomizer::insert_complete(uint64_t oldest_attestation,
                                   transaction::compact_tx&& tx)
        -> std::optional<cbdc::watchtower::tx_error> {
        const auto height_offset = get_notification_offset(oldest_attestation);

        auto offset_err = check_notification_offset(height_offset, tx);
        if(offset_err) {
            return offset_err;
        }

        auto cache_check_range = height_offset;

        auto err_set = check_stxo_cache(tx, cache_check_range);
        if(err_set) {
            return err_set;
        }

        add_tx_to_stxo_cache(tx);

        m_complete_txs.push_back(std::move(tx));

        return std::nullopt;
    }

    auto atomizer::pending_transactions() const -> size_t {
        return m_complete_txs.size();
    }

    auto atomizer::height() const -> uint64_t {
        return m_best_height;
    }

    atomizer::atomizer(const uint64_t best_height,
                       const size_t stxo_cache_depth)
        : m_best_height(best_height),
          m_spent_cache_depth(stxo_cache_depth) {
        m_txs.resize(stxo_cache_depth + 1);
        m_spent.resize(stxo_cache_depth + 1);
    }

    auto atomizer::serialize() -> cbdc::buffer {
        auto buf = cbdc::buffer();
        auto ser = cbdc::buffer_serializer(buf);

        ser << static_cast<uint64_t>(m_spent_cache_depth) << m_best_height
            << m_complete_txs << m_spent << m_txs;

        return buf;
    }

    void atomizer::deserialize(cbdc::serializer& buf) {
        m_complete_txs.clear();

        m_spent.clear();

        m_txs.clear();

        buf >> m_spent_cache_depth >> m_best_height >> m_complete_txs
            >> m_spent >> m_txs;
    }

    auto atomizer::operator==(const atomizer& other) const -> bool {
        return m_txs == other.m_txs && m_complete_txs == other.m_complete_txs
            && m_spent == other.m_spent && m_best_height == other.m_best_height
            && m_spent_cache_depth == other.m_spent_cache_depth;
    }

    auto atomizer::get_notification_offset(uint64_t block_height) const
        -> uint64_t {
        // Calculate the offset from the current block height when the shard
        // attested to this transaction.
        return m_best_height - block_height;
    }

    auto atomizer::check_notification_offset(uint64_t height_offset,
                                             const transaction::compact_tx& tx)
        const -> std::optional<cbdc::watchtower::tx_error> {
        // Check whether this TX notification is recent enough that we can
        // safely process it by checking our spent UTXO caches.
        if(height_offset > m_spent_cache_depth && !tx.m_inputs.empty()) {
            return cbdc::watchtower::tx_error{
                tx.m_id,
                cbdc::watchtower::tx_error_stxo_range{}};
        }
        return std::nullopt;
    }

    auto atomizer::check_stxo_cache(const transaction::compact_tx& tx,
                                    uint64_t cache_check_range) const
        -> std::optional<cbdc::watchtower::tx_error> {
        // For each height offset in our STXO cache up to the offset of the
        // oldest attestation we're using, check that the inputs have not
        // already been spent.
        auto err_set = std::unordered_set<hash_t, hashing::null>{};
        for(size_t offset = 0; offset <= cache_check_range; offset++) {
            for(const auto& inp : tx.m_inputs) {
                if(m_spent[offset].find(inp) != m_spent[offset].end()) {
                    err_set.insert(inp);
                }
            }
        }

        if(!err_set.empty()) {
            return cbdc::watchtower::tx_error{
                tx.m_id,
                cbdc::watchtower::tx_error_inputs_spent{std::move(err_set)}};
        }

        return std::nullopt;
    }

    void atomizer::add_tx_to_stxo_cache(const transaction::compact_tx& tx) {
        // None of the inputs have previously been spent during block heights
        // we used attestations from, so spend all the TX inputs in the current
        // block height (offset 0).
        m_spent[0].insert(tx.m_inputs.begin(), tx.m_inputs.end());
    }
}
