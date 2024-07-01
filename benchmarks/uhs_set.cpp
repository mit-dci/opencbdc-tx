// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/transaction.hpp"
#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"
#include "uhs/twophase/locking_shard/locking_shard.hpp"
#include "util/common/hash.hpp"
#include "util/common/hashmap.hpp"
#include "util/common/snapshot_map.hpp"

#include <benchmark/benchmark.h>
#include <gtest/gtest.h>
#include <unordered_set>
#include <variant>

class uhs_set : public ::benchmark::Fixture {
  public:
    uhs_set() {
        Iterations(10000);
    }

  protected:
    void SetUp(const ::benchmark::State&) override {
        m_uhs.snapshot();

        auto mint_tx1 = wallet1.mint_new_coins(1, 100);
        wallet1.confirm_transaction(mint_tx1);
        auto cp_1 = cbdc::transaction::compact_tx(mint_tx1);
        auto id_1 = cbdc::transaction::calculate_uhs_id(cp_1.m_outputs[0]);
        uhs_element el_1 {cp_1.m_outputs[0], epoch++, std::nullopt};
        m_uhs.emplace(id_1, el_1);

        auto mint_tx2 = wallet2.mint_new_coins(1, 100);
        wallet2.confirm_transaction(mint_tx2);
        auto cp_2 = cbdc::transaction::compact_tx(mint_tx2);
        auto id_2 = cbdc::transaction::calculate_uhs_id(cp_2.m_outputs[0]);
        uhs_element el_2 {cp_2.m_outputs[0], epoch++, std::nullopt};
        m_uhs.emplace(id_2, el_2);
    }

    cbdc::transaction::wallet wallet1;
    cbdc::transaction::wallet wallet2;

    cbdc::transaction::full_tx m_valid_tx{};
    cbdc::transaction::compact_tx m_cp_tx{};

    using uhs_element = cbdc::locking_shard::locking_shard::uhs_element;
    cbdc::snapshot_map<cbdc::hash_t, uhs_element> m_uhs{};
    size_t epoch{0};
};

// minimal tx execution
BENCHMARK_F(uhs_set, swap)(benchmark::State& state) {
    for(auto _ : state) {
        m_valid_tx = wallet1.send_to(2, wallet1.generate_key(), true).value();
        wallet1.confirm_transaction(m_valid_tx);
        m_cp_tx = cbdc::transaction::compact_tx(m_valid_tx);

        state.ResumeTiming();
        for(const auto& inp : m_cp_tx.m_inputs) {
            m_uhs.erase(inp);
        }
        for(const auto& outp : m_cp_tx.m_outputs) {
            const auto& uhs_id = cbdc::transaction::calculate_uhs_id(outp);
            uhs_element el{outp, epoch, std::nullopt};
            m_uhs.emplace(uhs_id, el);
        }
        state.PauseTiming();
        epoch++;
    }
}
