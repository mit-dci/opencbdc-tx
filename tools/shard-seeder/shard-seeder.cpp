// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/transaction.hpp"
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
    static constexpr auto min_arg_count = 5;
    if(args.size() < min_arg_count) {
        std::cout
            << "Usage: shard-seeder [number of shards] [number of utxos] "
               "[utxo value] [witness_commitment_hex] [mode]\n\n              "
               "           where [mode] = 0 (Atomizer), 1 (Two-phase commit)"
            << std::endl;
        return -1;
    }

    auto start = std::chrono::system_clock::now();
    ;
    auto num_shards = std::stoi(args[1]);
    auto num_utxos = std::stoi(args[2]);
    auto utxo_val = std::stoi(args[3]);
    auto witness_commitment_str = args[4];
    static constexpr auto mode_arg_idx = 5;
    auto mode = std::stoi(args[mode_arg_idx]);
    cbdc::hash_t witness_commitment
        = cbdc::hash_from_hex(witness_commitment_str);

    cbdc::transaction::wallet wal;
    wal.seed_readonly(witness_commitment, utxo_val, 0, num_utxos);

    auto shard_range
        = (std::numeric_limits<cbdc::config::shard_range_t::first_type>::max()
           + 1)
        / num_shards;
    auto gen_threads = std::vector<std::thread>(num_shards);
    for(int i = 0; i < num_shards; i++) {
        std::thread t(
            [&](int shard_idx) {
                auto shard_start = shard_idx * shard_range;
                auto shard_end = (shard_idx + 1) * shard_range - 1;
                if(shard_idx == num_shards - 1) {
                    shard_end = std::numeric_limits<
                        cbdc::config::shard_range_t::first_type>::max();
                }

                std::stringstream shard_db_dir;
                if(mode == 1) {
                    shard_db_dir << "2pc_";
                }

                shard_db_dir << "shard_preseed_" << num_utxos << "_"
                             << shard_start << "_" << shard_end;

                logger.info("Starting seeding of shard ",
                            shard_idx,
                            " to database ",
                            shard_db_dir.str());

                if(mode == 0) {
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
                    for(auto tx_idx = 0; tx_idx != num_utxos; tx_idx++) {
                        tx.m_inputs[0].m_prevout.m_index = tx_idx;
                        cbdc::transaction::compact_tx ctx(tx);
                        const cbdc::hash_t& output_hash = ctx.m_uhs_outputs[0];
                        if(output_hash[0] >= shard_start
                           && output_hash[0] <= shard_end) {
                            std::array<char, sizeof(output_hash)> hash_arr{};
                            std::memcpy(hash_arr.data(),
                                        output_hash.data(),
                                        sizeof(output_hash));
                            leveldb::Slice hash_key(hash_arr.data(),
                                                    output_hash.size());
                            batch.Put(hash_key, leveldb::Slice());
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
                } else if(mode == 1) { // 2PC Shard
                    auto out
                        = std::ofstream(shard_db_dir.str(), std::ios::binary);
                    size_t count = 0;
                    // write dummy size
                    auto ser = cbdc::ostream_serializer(out);
                    ser << count;
                    auto tx = wal.create_seeded_transaction(0).value();
                    for(auto tx_idx = 0; tx_idx != num_utxos; tx_idx++) {
                        tx.m_inputs[0].m_prevout.m_index = tx_idx;
                        cbdc::transaction::compact_tx ctx(tx);
                        const cbdc::hash_t& output_hash = ctx.m_uhs_outputs[0];
                        if(output_hash[0] >= shard_start
                           && output_hash[0] <= shard_end) {
                            ser << output_hash;
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

    for(int i = 0; i < num_shards; i++) {
        gen_threads[i].join();
    }
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now() - start)
                        .count();
    logger.info("Done in ", duration, "ms");
}
