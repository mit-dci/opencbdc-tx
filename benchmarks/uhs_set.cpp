// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/transaction.hpp"
#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"
#include "util/common/hash.hpp"
#include "util/common/hashmap.hpp"

#include <benchmark/benchmark.h>
#include <gtest/gtest.h>
#include <unordered_set>
#include <variant>

class uhs_set : public ::benchmark::Fixture {
  protected:
    void SetUp(const ::benchmark::State&) override {
        auto mint_tx1 = wallet1.mint_new_coins(1, 100);
        wallet1.confirm_transaction(mint_tx1);
        auto mint_tx2 = wallet2.mint_new_coins(1, 100);
        wallet2.confirm_transaction(mint_tx2);
        m_cp_tx = cbdc::transaction::compact_tx(m_valid_tx);
    }

    cbdc::transaction::wallet wallet1;
    cbdc::transaction::wallet wallet2;

    cbdc::transaction::full_tx m_valid_tx{};
    cbdc::transaction::compact_tx m_cp_tx;

    std::unordered_set<cbdc::hash_t, cbdc::hashing::null> set;
};

// benchmark how long it takes to emplace new values into an unordered set
BENCHMARK_F(uhs_set, emplace_new)(benchmark::State& state) {
    for(auto _ : state) {
        m_valid_tx = wallet1.send_to(2, wallet1.generate_key(), true).value();
        wallet1.confirm_transaction(m_valid_tx);
        m_cp_tx = cbdc::transaction::compact_tx(m_valid_tx);

        state.ResumeTiming();
        set.emplace(m_cp_tx.m_id);
        state.PauseTiming();

        m_cp_tx = cbdc::transaction::compact_tx(m_valid_tx);
    }
}

// benchmark how long it takes to remove values from an unordered set
BENCHMARK_F(uhs_set, erase_item)(benchmark::State& state) {
    for(auto _ : state) {
        m_valid_tx = wallet1.send_to(2, wallet1.generate_key(), true).value();
        wallet1.confirm_transaction(m_valid_tx);
        m_cp_tx = cbdc::transaction::compact_tx(m_valid_tx);
        set.emplace(m_cp_tx.m_id);
        state.ResumeTiming();
        set.erase(m_cp_tx.m_id);
        state.PauseTiming();

        m_cp_tx = cbdc::transaction::compact_tx(m_valid_tx);
    }
}
