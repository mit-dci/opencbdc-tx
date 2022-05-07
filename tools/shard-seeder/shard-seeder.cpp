// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/transaction.hpp"
#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"
#include "util/common/config.hpp"
#include "util/serialization/buffer_serializer.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/ostream_serializer.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <thread>

static constexpr int leveldb_buffer_size
    = 16 * 1024 * 1024; // 16MB can hold ~ 500K UHS_IDs
static constexpr int write_batch_size
    = 450000; // well within the write buffer size

auto get_2pc_uhs_key(const cbdc::hash_t& uhs_id) -> std::string {
    auto ret = std::string();
    ret.resize(uhs_id.size() + 1);
    std::memcpy(&ret[1], uhs_id.data(), uhs_id.size());
    ret.front() = 'u';
    return ret;
}

auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    auto logger = cbdc::logging::log(cbdc::logging::log_level::info);
    static constexpr auto min_arg_count = 2;
    if(args.size() < min_arg_count) {
        std::cout << "Usage: shard-seeder [config file]" << std::endl;
        return -1;
    }

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        logger.error("Error loading config file:",
                     std::get<std::string>(cfg_or_err));
        return -1;
    }
    auto cfg = std::get<cbdc::config::options>(cfg_or_err);

    auto start = std::chrono::system_clock::now();

    auto unique_ranges
        = std::vector<cbdc::config::shard_range_t>(cfg.m_shard_ranges);
    std::sort(unique_ranges.begin(), unique_ranges.end());
    unique_ranges.erase(unique(unique_ranges.begin(), unique_ranges.end()),
                        unique_ranges.end());
    auto num_shards = unique_ranges.size();
    auto num_utxos = cfg.m_seed_to - cfg.m_seed_from;
    auto utxo_val = cfg.m_seed_value;
    if(!cfg.m_seed_privkey.has_value()) {
        logger.error("Seed private key not specified");
        return -1;
    }
    auto secp_context = std::unique_ptr<secp256k1_context,
                                        decltype(&secp256k1_context_destroy)>(
        secp256k1_context_create(SECP256K1_CONTEXT_SIGN),
        &secp256k1_context_destroy);
    auto pubkey = cbdc::pubkey_from_privkey(cfg.m_seed_privkey.value(),
                                            secp_context.get());
    auto witness_commitment
        = cbdc::transaction::validation::get_p2pk_witness_commitment(pubkey);

    cbdc::transaction::wallet wal;
    wal.seed_readonly(witness_commitment, utxo_val, 0, num_utxos);

    auto shard_range
        = (std::numeric_limits<cbdc::config::shard_range_t::first_type>::max()
           + 1)
        / num_shards;
    auto gen_threads = std::vector<std::thread>(num_shards);
    for(size_t i = 0; i < num_shards; i++) {
        std::thread t(
            [&](size_t shard_idx) {
                auto shard_start = shard_idx * shard_range;
                auto shard_end = (shard_idx + 1) * shard_range - 1;
                if(shard_idx == num_shards - 1) {
                    shard_end = std::numeric_limits<
                        cbdc::config::shard_range_t::first_type>::max();
                }

                std::stringstream shard_db_dir;
                if(cfg.m_twophase_mode) {
                    shard_db_dir << "2pc_";
                }

                shard_db_dir << "shard_preseed_" << num_utxos << "_"
                             << shard_idx;

                logger.info("Starting seeding of shard ",
                            shard_idx,
                            " to database ",
                            shard_db_dir.str());

                if(!cfg.m_twophase_mode) {
                    leveldb::Options opt;
                    opt.create_if_missing = true;
                    opt.write_buffer_size = leveldb_buffer_size;

                    leveldb::WriteOptions wopt;

                    leveldb::DB* db_ptr{};
                    const auto res
                        = leveldb::DB::Open(opt, shard_db_dir.str(), &db_ptr);
                    auto db = std::unique_ptr<leveldb::DB>(db_ptr);
                    if(!res.ok()) {
                        logger.error("Failed to open shard DB ",
                                     shard_db_dir.str(),
                                     " for shard ",
                                     shard_idx,
                                     ": ",
                                     res.ToString());
                        return;
                    }
                    auto tx = wal.create_seeded_transaction(0).value();
                    auto batch_size = 0;
                    leveldb::WriteBatch batch;
                    for(size_t tx_idx = 0; tx_idx != num_utxos; tx_idx++) {
                        tx.m_inputs[0].m_prevout.m_index = tx_idx;
                        cbdc::transaction::compact_tx ctx(tx);
                        const cbdc::hash_t& output_hash
                            = ctx.m_outputs[0].m_id;
                        if(output_hash[0] >= shard_start
                           && output_hash[0] <= shard_end) {
                            std::array<char, sizeof(output_hash)> hash_arr{};
                            std::memcpy(hash_arr.data(),
                                        output_hash.data(),
                                        sizeof(output_hash));
                            static constexpr auto aux_size =
                                sizeof(ctx.m_outputs[0].m_auxiliary);
                            static constexpr auto rng_size =
                                sizeof(ctx.m_outputs[0].m_range);
                            static constexpr auto cst_size =
                                sizeof(ctx.m_outputs[0].m_consistency);
                            std::array<char, aux_size + rng_size + cst_size>
                                proofs_arr{};

                            std::memcpy(proofs_arr.data(),
                                        ctx.m_outputs[0].m_auxiliary.data(),
                                        aux_size);
                            std::memcpy(proofs_arr.data() + aux_size,
                                        ctx.m_outputs[0].m_range.data(),
                                        rng_size);
                            std::memcpy(proofs_arr.data() + aux_size + rng_size,
                                        ctx.m_outputs[0].m_consistency.data(),
                                        cst_size);
                            leveldb::Slice hash_key(hash_arr.data(),
                                                    output_hash.size());
                            leveldb::Slice ProofVal(proofs_arr.data(),
                                                    proofs_arr.size());

                            batch.Put(hash_key, ProofVal);
                            batch_size++;
                            if(batch_size >= write_batch_size) {
                                db->Write(wopt, &batch);
                                batch.Clear();
                                batch_size = 0;
                            }
                        }
                    }
                    if(batch_size > 0) {
                        db->Write(wopt, &batch);
                    }
                    logger.info("Shard ", shard_idx, " succesfully seeded");
                } else if(cfg.m_twophase_mode) { // 2PC Shard
                    auto out
                        = std::ofstream(shard_db_dir.str(), std::ios::binary);
                    size_t count = 0;
                    // write dummy size
                    auto ser = cbdc::ostream_serializer(out);
                    ser << count;
                    auto tx = wal.create_seeded_transaction(0).value();
                    for(size_t tx_idx = 0; tx_idx != num_utxos; tx_idx++) {
                        tx.m_inputs[0].m_prevout.m_index = tx_idx;
                        cbdc::transaction::compact_tx ctx(tx);
                        const auto& compact_out = ctx.m_outputs[0];
                        if(compact_out.m_id[0] >= shard_start
                           && compact_out.m_id[0] <= shard_end) {
                            ser << compact_out.m_id;
                            ser << compact_out;
                            count++;
                        }
                    }
                    ser.reset();
                    ser << count;
                }
            },
            i);
        gen_threads[i] = std::move(t);
    }

    for(size_t i = 0; i < num_shards; i++) {
        gen_threads[i].join();
    }
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now() - start)
                        .count();
    logger.info("Done in ", duration, "ms");
}
