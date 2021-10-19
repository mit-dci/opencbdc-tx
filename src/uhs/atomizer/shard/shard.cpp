// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "shard.hpp"

#include <utility>

namespace cbdc::shard {
    shard::shard(config::shard_range_t prefix_range)
        : m_prefix_range(std::move(prefix_range)) {}

    auto shard::open_db(const std::string& db_dir)
        -> std::optional<std::string> {
        leveldb::Options opt;
        opt.create_if_missing = true;

        leveldb::DB* db_ptr{};
        const auto res = leveldb::DB::Open(opt, db_dir, &db_ptr);

        if(!res.ok()) {
            return res.ToString();
        }
        this->m_db.reset(db_ptr);

        // Read best block height from database or initialize it to zero
        std::string bestBlockHeight;
        const auto bestBlockRes = this->m_db->Get(this->m_read_options,
                                                  m_best_block_height_key,
                                                  &bestBlockHeight);
        if(bestBlockRes.IsNotFound()) {
            this->m_best_block_height = 0;
            std::array<char, sizeof(m_best_block_height)> height_arr{};
            std::memcpy(height_arr.data(),
                        &m_best_block_height,
                        sizeof(m_best_block_height));
            leveldb::Slice startBestBlockHeight(
                height_arr.data(),
                sizeof(this->m_best_block_height));
            this->m_db->Put(this->m_write_options,
                            m_best_block_height_key,
                            startBestBlockHeight);
        } else {
            assert(bestBlockHeight.size()
                   == sizeof(this->m_best_block_height));
            std::memcpy(&this->m_best_block_height,
                        bestBlockHeight.c_str(),
                        sizeof(this->m_best_block_height));
        }

        auto snp = get_snapshot();
        update_snapshot(std::move(snp));

        return std::nullopt;
    }

    auto shard::digest_block(const cbdc::atomizer::block& blk) -> bool {
        if(blk.m_height != m_best_block_height + 1) {
            return false;
        }

        leveldb::WriteBatch batch;

        // Iterate over all confirmed transactions
        for(const auto& tx : blk.m_transactions) {
            // Add new outputs
            for(const auto& out : tx.m_outputs) {
                if(is_output_on_shard(out.m_id)) {
                    std::array<char, sizeof(out.m_id)> out_arr{};
                    std::memcpy(out_arr.data(),
                                out.m_id.data(),
                                out.m_id.size());
                    leveldb::Slice OutPointKey(out_arr.data(),
                                               out.m_id.size());

                    static constexpr auto aux_size = sizeof(out.m_auxiliary);
                    static constexpr auto rng_size = sizeof(out.m_range);
                    static constexpr auto preimg_size = sizeof(out.m_range);

                    std::array<char, aux_size + rng_size + preimg_size>
                        proofs_arr{};
                    std::memcpy(proofs_arr.data(),
                                out.m_auxiliary.data(),
                                aux_size);
                    std::memcpy(proofs_arr.data() + aux_size,
                                out.m_range.data(),
                                out.m_range.size());
                    std::memcpy(proofs_arr.data() + aux_size + rng_size,
                                out.m_provenance.data(),
                                out.m_provenance.size());

                    leveldb::Slice ProofVal(proofs_arr.data(),
                                            proofs_arr.size());
                    batch.Put(OutPointKey, ProofVal);
                }
            }

            // Delete spent inputs
            for(const auto& inp : tx.m_inputs) {
                if(is_output_on_shard(inp)) {
                    std::array<char, sizeof(inp)> inp_arr{};
                    std::memcpy(inp_arr.data(), inp.data(), inp.size());
                    leveldb::Slice OutPointKey(inp_arr.data(), inp.size());
                    batch.Delete(OutPointKey);
                }
            }
        }

        // Bump the best block height
        this->m_best_block_height++;
        std::array<char, sizeof(m_best_block_height)> height_arr{};
        std::memcpy(height_arr.data(),
                    &m_best_block_height,
                    sizeof(m_best_block_height));
        leveldb::Slice newBestBlockHeight(height_arr.data(),
                                          sizeof(this->m_best_block_height));
        batch.Put(m_best_block_height_key, newBestBlockHeight);

        // Commit the changes atomically
        this->m_db->Write(this->m_write_options, &batch);

        auto snp = get_snapshot();
        update_snapshot(std::move(snp));

        return true;
    }

    auto shard::digest_transaction(transaction::compact_tx tx)
        -> std::variant<atomizer::tx_notify_request,
                        cbdc::watchtower::tx_error> {
        std::shared_ptr<const leveldb::Snapshot> snp{};
        uint64_t snp_height{};
        {
            std::shared_lock<std::shared_mutex> l(m_snp_mut);
            snp_height = m_snp_height;
            snp = m_snp;
        }

        // Don't process transactions until we've heard from the atomizer
        if(snp_height == 0) {
            return cbdc::watchtower::tx_error{
                tx.m_id,
                cbdc::watchtower::tx_error_sync{}};
        }

        if(tx.m_inputs.empty()) {
            return cbdc::watchtower::tx_error{
                tx.m_id,
                cbdc::watchtower::tx_error_inputs_dne{{}}};
        }

        auto read_options = m_read_options;
        read_options.snapshot = snp.get();

        // Check TX inputs exist
        std::unordered_set<uint64_t> attestations;
        std::vector<hash_t> dne_inputs;
        for(uint64_t i = 0; i < tx.m_inputs.size(); i++) {
            const auto& inp = tx.m_inputs[i];
            // Only check for inputs/outputs relevant to this shard
            if(!is_output_on_shard(inp)) {
                continue;
            }

            std::array<char, sizeof(inp)> inp_arr{};
            std::memcpy(inp_arr.data(), inp.data(), inp.size());
            leveldb::Slice OutPointKey(inp_arr.data(), inp.size());
            std::string op;

            const auto& res = m_db->Get(read_options, OutPointKey, &op);
            if(res.IsNotFound()) {
                dne_inputs.push_back(inp);
            } else {
                attestations.insert(i);
            }
        }

        if(!dne_inputs.empty()) {
            return cbdc::watchtower::tx_error{
                tx.m_id,
                cbdc::watchtower::tx_error_inputs_dne{dne_inputs}};
        }

        atomizer::tx_notify_request msg;
        msg.m_attestations = std::move(attestations);
        msg.m_tx = std::move(tx);
        msg.m_block_height = snp_height;

        return msg;
    }

    auto shard::best_block_height() const -> uint64_t {
        return m_best_block_height;
    }

    auto shard::is_output_on_shard(const hash_t& uhs_hash) const -> bool {
        return config::hash_in_shard_range(m_prefix_range, uhs_hash);
    }

    void shard::update_snapshot(std::shared_ptr<const leveldb::Snapshot> snp) {
        std::unique_lock<std::shared_mutex> l(m_snp_mut);
        m_snp_height = m_best_block_height;
        m_snp = std::move(snp);
    }

    auto shard::audit(const std::shared_ptr<const leveldb::Snapshot>& snp)
        -> std::optional<uint64_t> {
        auto opts = leveldb::ReadOptions();
        opts.snapshot = snp.get();
        auto it = std::shared_ptr<leveldb::Iterator>(m_db->NewIterator(opts));
        it->SeekToFirst();
        // Skip best block height key
        it->Next();
        uint64_t tot{};
        for(; it->Valid(); it->Next()) {
            auto key = it->key();
            auto buf = cbdc::buffer();
            buf.extend(key.size());
            std::memcpy(buf.data(), key.data(), key.size());
            auto maybe_uhs_element
                = cbdc::from_buffer<transaction::uhs_element>(buf);
            if(!maybe_uhs_element.has_value()) {
                return std::nullopt;
            }
            auto& uhs_element = maybe_uhs_element.value();
            if(transaction::calculate_uhs_id(uhs_element.m_data,
                                             uhs_element.m_value)
               != uhs_element.m_id) {
                return std::nullopt;
            }
            tot += uhs_element.m_value;
        }

        return tot;
    }

    auto shard::get_snapshot() -> std::shared_ptr<const leveldb::Snapshot> {
        auto snp = std::shared_ptr<const leveldb::Snapshot>(
            m_db->GetSnapshot(),
            [&](const leveldb::Snapshot* p) {
                m_db->ReleaseSnapshot(p);
            });
        return snp;
    }
}
