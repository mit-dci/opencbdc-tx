// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/transaction.hpp"
#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"

#include <benchmark/benchmark.h>
#include <gtest/gtest.h>

#define SWEEP_MAX 32

// reset wallets to default status
void reset_wallets(cbdc::transaction::wallet& w1,
                   cbdc::transaction::wallet& w2,
                   uint32_t init_count) {
    cbdc::transaction::full_tx m_valid_tx{};
    if(w1.balance() + w2.balance() < init_count * 2) {
        auto mint_tx = w1.mint_new_coins(1,
                                         init_count * 2
                                             - (w1.balance() + w2.balance()));
        w1.confirm_transaction(mint_tx);
    }
    if(w2.balance() > 0) {
        m_valid_tx = w2.send_to(w2.balance(), w1.generate_key(), true).value();
        w1.confirm_transaction(m_valid_tx);
        w2.confirm_transaction(m_valid_tx);
    }
    if(w1.count() != init_count) {
        // generate an N-1 transaction
        m_valid_tx = w1.send_to(w1.balance(), w1.generate_key(), true).value();
        w1.confirm_transaction(m_valid_tx);
        // fan to 10 (value-2 UTXOs)
        m_valid_tx = w1.fan(init_count, 2, w1.generate_key(), true).value();
        w1.confirm_transaction(m_valid_tx);
    }
}

/// @brief Time an N-in, 1-out transaction.
/// @brief Note: handles benchmark timing, do not time outside function.
/// @param sender
/// @param reciever
/// @param n_in
/// @param state
/// @return
inline bool generate_Nto1_tx(cbdc::transaction::wallet& sender,
                             cbdc::transaction::wallet& reciever,
                             uint32_t n_in,
                             benchmark::State& state) {
    std::optional<cbdc::transaction::full_tx> maybe_tx{};
    state.ResumeTiming();
    maybe_tx = sender.send_to(n_in * 2, reciever.generate_key(), true).value();
    state.PauseTiming();
    if(maybe_tx.has_value()) {
        sender.confirm_transaction(*maybe_tx);
        reciever.confirm_transaction(*maybe_tx);
        return true;
    }
    return false;
}

/// @brief Time an N-in, 2-out transaction.
/// @brief Note: handles benchmark timing, do not time outside function.
/// @param sender
/// @param reciever
/// @param n_in
/// @param state
/// @return
inline bool generate_Nto2_tx(cbdc::transaction::wallet& sender,
                             cbdc::transaction::wallet& reciever,
                             uint32_t n_in,
                             benchmark::State& state) {
    std::optional<cbdc::transaction::full_tx> maybe_tx{};
    state.ResumeTiming();
    maybe_tx
        = sender.send_to(n_in * 2 - 1, reciever.generate_key(), true).value();
    state.PauseTiming();
    if(maybe_tx.has_value()) {
        sender.confirm_transaction(*maybe_tx);
        reciever.confirm_transaction(*maybe_tx);
        return true;
    }
    return false;
}

// Benchmarkable N in 1 out transaction (for sweep)
static void Nto1_tx(benchmark::State& state) {
    cbdc::transaction::wallet wallet_a;
    cbdc::transaction::wallet wallet_b;
    bool valid;
    for(auto _ : state) {
        reset_wallets(wallet_a, wallet_b, SWEEP_MAX);
        valid = generate_Nto1_tx(wallet_a, wallet_b, state.range(0), state);
        if(!valid) {
            GTEST_LOG_(ERROR) << state.range(0) << "-2 transaction invalid";
            return;
        }
        ASSERT_EQ(wallet_a.balance(), SWEEP_MAX * 2 - 2 * state.range(0));
        ASSERT_EQ(wallet_a.count(), SWEEP_MAX - state.range(0));
        ASSERT_EQ(wallet_b.balance(), state.range(0) * 2);
        ASSERT_EQ(wallet_b.count(), 1);
    }
    state.SetComplexityN(state.range(0));
}

// Benchmarkable N in 2 out transaction (for sweep)
static void Nto2_tx(benchmark::State& state) {
    cbdc::transaction::wallet wallet_a;
    cbdc::transaction::wallet wallet_b;
    bool valid;
    for(auto _ : state) {
        reset_wallets(wallet_a, wallet_b, SWEEP_MAX);
        valid = generate_Nto2_tx(wallet_a, wallet_b, state.range(0), state);
        if(!valid) {
            GTEST_LOG_(ERROR) << state.range(0) << "-2 transaction invalid";
            return;
        }
        ASSERT_EQ(wallet_a.balance(), SWEEP_MAX * 2 - 2 * state.range(0) + 1);
        ASSERT_EQ(wallet_a.count(), SWEEP_MAX - state.range(0) + 1);
        ASSERT_EQ(wallet_b.balance(), state.range(0) * 2 - 1);
        ASSERT_EQ(wallet_b.count(), 1);
    }
    state.SetComplexityN(state.range(0));
}

// Benchmark declarations
BENCHMARK(Nto1_tx)
    ->RangeMultiplier(2)
    ->Range(1, SWEEP_MAX)
    ->Complexity(benchmark::oAuto);

BENCHMARK(Nto2_tx)
    ->RangeMultiplier(2)
    ->Range(1, SWEEP_MAX)
    ->Complexity(benchmark::oAuto);

