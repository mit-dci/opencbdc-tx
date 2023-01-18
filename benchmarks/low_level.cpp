// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Note: Contains call to BENCHMARK_MAIN

#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/transaction.hpp"
#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"
#include "util/serialization/istream_serializer.hpp"
#include "util/serialization/ostream_serializer.hpp"

#include <benchmark/benchmark.h>
#include <filesystem>
#include <gtest/gtest.h>

class low_level : public ::benchmark::Fixture {
  protected:
    void SetUp(const ::benchmark::State&) override {
        auto mint_tx1 = wallet1.mint_new_coins(100, 2);
        wallet1.confirm_transaction(mint_tx1);

        m_of.open(m_BENCHMARK_File);

        counter = 1;
    }

    void TearDown(const ::benchmark::State&) override {
        m_if.close();
        m_of.close();
        std::filesystem::remove_all(m_BENCHMARK_File);
    };

    cbdc::transaction::wallet wallet1;
    cbdc::transaction::wallet wallet2;

    cbdc::transaction::full_tx m_valid_tx{};
    cbdc::transaction::full_tx m_valid_tx_multi_inp{};

    // for serialization
    std::ifstream m_if;
    std::ofstream m_of;

    cbdc::istream_serializer m_is{m_if};
    cbdc::ostream_serializer m_os{m_of};

    static constexpr auto m_BENCHMARK_File = "serial_BENCHMARK_File.dat";

    // used to ensure that transaction validation timing
    // uses unique N values for N->1 transactions
    int counter = 1;
};

// serialize full tx
BENCHMARK_F(low_level, serialize_tx)(benchmark::State& state) {
    m_valid_tx = wallet1.send_to(2, wallet2.generate_key(), true).value();
    for(auto _ : state) {
        m_os << m_valid_tx;
    }
    m_of.close();
}

// serialize compact tx
BENCHMARK_F(low_level, serialize_compact_tx)(benchmark::State& state) {
    // serialize 1-1 tx
    m_valid_tx = wallet1.send_to(2, wallet2.generate_key(), true).value();
    auto cp_tx = cbdc::transaction::compact_tx(m_valid_tx);
    for(auto _ : state) {
        m_os << cp_tx;
    }
    m_of.close();
}

// deserialize full tx
BENCHMARK_F(low_level, deserialize_tx)(benchmark::State& state) {
    // serialize 1-1 tx
    auto read_tx = cbdc::transaction::full_tx();
    m_valid_tx = wallet1.send_to(2, wallet2.generate_key(), true).value();
    for(auto _ : state) {
        state.PauseTiming();
        m_os.reset();
        m_os << m_valid_tx;
        m_of.close();
        m_if.open(m_BENCHMARK_File);

        read_tx = cbdc::transaction::full_tx();
        m_is.reset();
        state.ResumeTiming();
        m_is >> read_tx;
        state.PauseTiming();
        ASSERT_EQ(read_tx, m_valid_tx);
    }

    m_if.close();
}

// deserialize compact tx
BENCHMARK_F(low_level, deserialize_compact_tx)(benchmark::State& state) {
    // serialize 1-1 tx
    auto read_tx = cbdc::transaction::full_tx();
    auto read_cp = cbdc::transaction::compact_tx(read_tx);

    m_valid_tx = wallet1.send_to(2, wallet2.generate_key(), true).value();
    auto cp_tx = cbdc::transaction::compact_tx(m_valid_tx);
    for(auto _ : state) {
        state.PauseTiming();
        m_os.reset();
        m_os << cp_tx;
        m_of.close();
        m_if.open(m_BENCHMARK_File);

        read_cp = cbdc::transaction::compact_tx(read_tx);
        m_is.reset();
        state.ResumeTiming();
        m_is >> read_cp;
        state.PauseTiming();
        ASSERT_EQ(cp_tx, read_cp);
    }

    m_if.close();
}

// wallet sign tx
BENCHMARK_F(low_level, sign_tx)(benchmark::State& state) {
    // sign 1-1 tx
    for(auto _ : state) {
        state.PauseTiming();
        m_valid_tx = wallet1.send_to(2, wallet1.generate_key(), true).value();

        state.ResumeTiming();
        wallet1.sign(m_valid_tx);
        state.PauseTiming();

        wallet1.confirm_transaction(m_valid_tx);
    }
}

// tx validiation
BENCHMARK_F(low_level, valid_tx)(benchmark::State& state) {
    // validate n-1 tx
    auto err = cbdc::transaction::validation::check_tx(m_valid_tx);
    for(auto _ : state) {
        m_valid_tx = wallet1.mint_new_coins(counter, 1);
        wallet1.confirm_transaction(m_valid_tx);
        m_valid_tx
            = wallet1.send_to(counter, wallet2.generate_key(), true).value();
        state.ResumeTiming();
        err = cbdc::transaction::validation::check_tx(m_valid_tx);
        state.PauseTiming();
        counter++;
    }
}

// test quick-failing validation
BENCHMARK_F(low_level, no_inputs)(benchmark::State& state) {
    m_valid_tx.m_inputs.clear();
    auto err = cbdc::transaction::validation::check_tx(m_valid_tx);
    for(auto _ : state) {
        err = cbdc::transaction::validation::check_tx(m_valid_tx);
    }
}

// calculate uhs id
BENCHMARK_F(low_level, calculate_uhs_id)(benchmark::State& state) {
    m_valid_tx = wallet1.send_to(2, wallet2.generate_key(), true).value();
    auto cp_tx = cbdc::transaction::compact_tx(m_valid_tx);
    auto engine = std::default_random_engine();
    for(auto _ : state) {
        state.PauseTiming();
        uint64_t i = (uint64_t)engine();
        state.ResumeTiming();
        cbdc::transaction::uhs_id_from_output(cp_tx.m_id,
                                              i,
                                              m_valid_tx.m_outputs[0]);
    }
}

BENCHMARK_MAIN();
