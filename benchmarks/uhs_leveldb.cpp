// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/transaction.hpp"
#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"

#include <benchmark/benchmark.h>
#include <filesystem>
#include <gtest/gtest.h>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <vector>

static constexpr auto g_shard_test_dir = "test_shard_db";

// container for database variables
struct db_container {
    leveldb::DB* db_ptr{};
    leveldb::Options opt;
    leveldb::WriteBatch batch;
    leveldb::WriteOptions write_opt;
    leveldb::ReadOptions read_opt;

    std::unique_ptr<leveldb::DB> m_db;

    cbdc::transaction::wallet wallet1;
    cbdc::transaction::wallet wallet2;

    cbdc::transaction::full_tx m_valid_tx{};
    cbdc::transaction::compact_tx m_cp_tx;
    std::vector<cbdc::transaction::compact_tx> block;
    std::vector<cbdc::transaction::compact_tx> block_abridged;

    leveldb::Status res;

    // default constructor
    // this is intended to mimic what benchmark fixtures
    // do, while permitting benchmark modificaitons
    db_container() {
        opt.create_if_missing = true;

        auto mint_tx1 = wallet1.mint_new_coins(2, 100);
        auto mint_tx2 = wallet2.mint_new_coins(1, 100);
        wallet1.confirm_transaction(mint_tx1);
        wallet2.confirm_transaction(mint_tx2);

        res = leveldb::DB::Open(opt, g_shard_test_dir, &db_ptr);
        m_db.reset(db_ptr);

        block.push_back(cbdc::transaction::compact_tx(mint_tx1));
        block.push_back(cbdc::transaction::compact_tx(mint_tx2));

        m_valid_tx
            = wallet1.send_to(100, wallet2.generate_key(), true).value();
        block.push_back(cbdc::transaction::compact_tx(m_valid_tx));
        block_abridged.push_back(cbdc::transaction::compact_tx(m_valid_tx));

        for(int i = 0; i < 10; i++) {
            m_valid_tx
                = wallet1.send_to(100, wallet2.generate_key(), true).value();
            wallet1.confirm_transaction(m_valid_tx);
            wallet2.confirm_transaction(m_valid_tx);
            block.push_back(cbdc::transaction::compact_tx(m_valid_tx));

            m_valid_tx
                = wallet2.send_to(50, wallet1.generate_key(), true).value();
            wallet1.confirm_transaction(m_valid_tx);
            wallet2.confirm_transaction(m_valid_tx);
            block.push_back(cbdc::transaction::compact_tx(m_valid_tx));

            m_valid_tx
                = wallet2.send_to(50, wallet1.generate_key(), true).value();
            wallet1.confirm_transaction(m_valid_tx);
            wallet2.confirm_transaction(m_valid_tx);
            block.push_back(cbdc::transaction::compact_tx(m_valid_tx));
        }
    }

    void tear_down() {
        std::filesystem::remove_all(g_shard_test_dir);
    }
};

// test placing a new element in DB
static void uhs_leveldb_put_new(benchmark::State& state) {
    auto db = db_container();

    // simulate storage of a new set of transactions
    for(auto _ : state) {
        db.m_valid_tx
            = db.wallet2.send_to(50, db.wallet1.generate_key(), true).value();
        db.wallet1.confirm_transaction(db.m_valid_tx);
        db.wallet2.confirm_transaction(db.m_valid_tx);
        db.m_valid_tx
            = db.wallet1.send_to(50, db.wallet2.generate_key(), true).value();
        db.wallet1.confirm_transaction(db.m_valid_tx);
        db.wallet2.confirm_transaction(db.m_valid_tx);

        db.m_cp_tx = cbdc::transaction::compact_tx(db.m_valid_tx);
        std::array<char, sizeof(db.m_cp_tx.m_outputs)> out_arr{};
        std::memcpy(out_arr.data(),
                    db.m_cp_tx.m_outputs.data(),
                    db.m_cp_tx.m_outputs.size());
        leveldb::Slice OutPointKey(out_arr.data(),
                                   db.m_cp_tx.m_outputs.size());

        // actual storage
        state.ResumeTiming();
        db.res = db.m_db->Put(db.write_opt, OutPointKey, leveldb::Slice());
        state.PauseTiming();
    }

    db.tear_down();
}

// test deleting from database
static void uhs_leveldb_item_delete(benchmark::State& state) {
    auto db = db_container();

    db.m_cp_tx = cbdc::transaction::compact_tx(db.m_valid_tx);
    std::array<char, sizeof(db.m_cp_tx.m_outputs)> out_arr{};
    std::memcpy(out_arr.data(),
                db.m_cp_tx.m_outputs.data(),
                db.m_cp_tx.m_outputs.size());
    leveldb::Slice OutPointKey(out_arr.data(),
                               db.m_cp_tx.m_outputs.size());

    for(auto _ : state) {
        state.PauseTiming();
        db.m_db->Put(db.write_opt, OutPointKey, leveldb::Slice());
        state.ResumeTiming();
        db.m_db->Delete(db.write_opt, OutPointKey);
        state.PauseTiming();
    }

    db.tear_down();
}

// actual sim of shard storing block tx info
static void uhs_leveldb_shard_sim(benchmark::State& state) {
    auto db = db_container();

    for(auto _ : state) {
        state.PauseTiming();
        leveldb::WriteBatch batch;
        state.ResumeTiming();
        for(const auto& tx : db.block) {
            for(const auto& out : tx.m_outputs) {
                auto id = calculate_uhs_id(out);
                std::array<char, sizeof(id)> out_arr{};
                std::memcpy(out_arr.data(), id.data(), id.size());
                leveldb::Slice OutPointKey(out_arr.data(), id.size());
                batch.Put(OutPointKey, leveldb::Slice());
            }
            for(const auto& inp : tx.m_inputs) {
                std::array<char, sizeof(inp)> inp_arr{};
                std::memcpy(inp_arr.data(), inp.data(), inp.size());
                leveldb::Slice OutPointKey(inp_arr.data(), inp.size());
                batch.Delete(OutPointKey);
            }
        }
        db.m_db->Write(db.write_opt, &batch);
        state.PauseTiming();
    }

    db.tear_down();
}

// abridged sim of shard storing block tx info (Batch storage of one TX)
static void uhs_leveldb_shard_sim_brief(benchmark::State& state) {
    auto db = db_container();
    for(auto _ : state) {
        state.PauseTiming();
        leveldb::WriteBatch batch;
        state.ResumeTiming();
        for(const auto& tx : db.block_abridged) {
            for(const auto& out : tx.m_outputs) {
                auto id = calculate_uhs_id(out);
                std::array<char, sizeof(id)> out_arr{};
                std::memcpy(out_arr.data(), id.data(), id.size());
                leveldb::Slice OutPointKey(out_arr.data(), id.size());
                batch.Put(OutPointKey, leveldb::Slice());
            }
            for(const auto& inp : tx.m_inputs) {
                std::array<char, sizeof(inp)> inp_arr{};
                std::memcpy(inp_arr.data(), inp.data(), inp.size());
                leveldb::Slice OutPointKey(inp_arr.data(), inp.size());
                batch.Delete(OutPointKey);
            }
        }
        db.m_db->Write(db.write_opt, &batch);
        state.PauseTiming();
    }

    db.tear_down();
}

BENCHMARK(uhs_leveldb_put_new)->Threads(1);
BENCHMARK(uhs_leveldb_item_delete)->Threads(1);
BENCHMARK(uhs_leveldb_shard_sim)->Threads(1);
BENCHMARK(uhs_leveldb_shard_sim_brief)->Threads(1);
