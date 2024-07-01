// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/transaction.hpp"
#include "uhs/twophase/locking_shard/locking_shard.hpp"
#include "util/common/hash.hpp"
#include "util/common/hashmap.hpp"
#include "util/common/keys.hpp"
#include "util/common/config.hpp"
#include "util/common/random_source.hpp"
#include "util/common/snapshot_map.hpp"

#include <secp256k1_bppp.h>
#include <benchmark/benchmark.h>
#include <gtest/gtest.h>
#include <unordered_map>
#include <random>
#include <unordered_set>

#define SWEEP_MAX 100000
#define EPOCH     1000

using namespace cbdc;
using secp256k1_context_destroy_type = void (*)(secp256k1_context*);
using uhs_element = locking_shard::locking_shard::uhs_element;

struct GensDeleter {
    explicit GensDeleter(secp256k1_context* ctx) : m_ctx(ctx) {}

    void operator()(secp256k1_bppp_generators* gens) const {
        secp256k1_bppp_generators_destroy(m_ctx, gens);
    }

    secp256k1_context* m_ctx;
};

static std::default_random_engine m_shuffle;

static const inline auto rnd
    = std::make_unique<random_source>(config::random_source);

static std::unique_ptr<secp256k1_context, secp256k1_context_destroy_type>
    secp{secp256k1_context_create(SECP256K1_CONTEXT_NONE),
         &secp256k1_context_destroy};

/// should be set to exactly `floor(log_base(value)) + 1`
///
/// We use n_bits = 64, base = 16, so this should always be 24.
static const inline auto generator_count = 16 + 8;

static std::unique_ptr<secp256k1_bppp_generators, GensDeleter>
    generators{
        secp256k1_bppp_generators_create(secp.get(),
                                         generator_count),
        GensDeleter(secp.get())};

static auto gen_map(uint64_t map_size, bool deleted = false) -> snapshot_map<hash_t, uhs_element> {
    std::uniform_int_distribution<uint64_t> dist(EPOCH - 100, EPOCH + 100);
    auto uhs = snapshot_map<hash_t, uhs_element>();

    auto comm = commit(secp.get(), 10, hash_t{}).value();
    auto rng
        = transaction::prove(secp.get(),
                             generators.get(),
                             *rnd,
                             {hash_t{}, 10},
                             &comm);
    auto commitment = serialize_commitment(secp.get(), comm);

    for(uint64_t i = 1; i <= map_size; i++) {
        transaction::compact_output out{commitment, rng, rnd->random_hash()};
        auto del = deleted ? std::optional<uint64_t>{dist(m_shuffle)} : std::nullopt;
        uhs_element el0{out, 0, del};
        auto key = transaction::calculate_uhs_id(out);
        uhs.emplace(key, el0);
    }
    return uhs;
}

static auto audit(snapshot_map<hash_t, uhs_element>& uhs,
                  snapshot_map<hash_t, uhs_element>& locked,
                  snapshot_map<hash_t, uhs_element>& spent)
    -> std::optional<commitment_t> {

    {
        uhs.snapshot();
        locked.snapshot();
        spent.snapshot();
    }

    bool failed = false;
    uint64_t epoch = EPOCH;

    static constexpr auto scratch_size = 8192UL * 1024UL;
    [[maybe_unused]] secp256k1_scratch_space* scratch
        = secp256k1_scratch_space_create(secp.get(), scratch_size);

    static constexpr size_t threshold = 100000;
    size_t cursor = 0;
    std::vector<commitment_t> comms{};
    auto* range_batch = secp256k1_bppp_rangeproof_batch_create(secp.get(), 34 * (threshold + 1));
    auto summarize
        = [&](const snapshot_map<hash_t, uhs_element>& m) {
        for(const auto& [id, elem] : m) {
            if(failed) {
                break;
            }
            if(elem.m_creation_epoch <= epoch
               && (!elem.m_deletion_epoch.has_value()
                   || (elem.m_deletion_epoch.value() > epoch))) {

                auto uhs_id
                    = transaction::calculate_uhs_id(elem.m_out);
                if(uhs_id != id) {
                    failed = true;
                }
                auto comm = elem.m_out.m_value_commitment;
                auto c = deserialize_commitment(secp.get(), comm).value();
                auto r = transaction::validation::range_batch_add(
                    *range_batch,
                    scratch,
                    elem.m_out.m_range,
                    c
                );
                if(!r.has_value()) {
                    ++cursor;
                }
                comms.push_back(comm);
            }
            if(cursor >= threshold) {
                failed = transaction::validation::check_range_batch(*range_batch).has_value();
                [[maybe_unused]] auto res = secp256k1_bppp_rangeproof_batch_clear(secp.get(), range_batch);
                cursor = 0;
            }
        }
        if(cursor > 0) {
            failed = transaction::validation::check_range_batch(*range_batch).has_value();
            [[maybe_unused]] auto res = secp256k1_bppp_rangeproof_batch_clear(secp.get(), range_batch);
            cursor = 0;
        }
    };

    summarize(uhs);
    summarize(locked);
    summarize(spent);
    [[maybe_unused]] auto res = secp256k1_bppp_rangeproof_batch_destroy(secp.get(), range_batch);
    free(range_batch);

    secp256k1_scratch_space_destroy(secp.get(), scratch);

//    std::vector<commitment_t> comms{};
//    comms.reserve(pool.size());
//
//    for(auto& f : pool) {
//        auto c = f.get();
//        failed = !c.has_value();
//        if(failed) {
//            break;
//        }
//        comms.emplace_back(std::move(c.value()));
//k   }

    {
        uhs.release_snapshot();
        locked.release_snapshot();
        spent.release_snapshot();
    }

    if(failed) {
        return std::nullopt;
    }

    return sum_commitments(secp.get(), comms);
}

static void audit_routine(benchmark::State& state) {
    auto key_count = state.range(0);

    auto seed = std::chrono::high_resolution_clock::now()
                    .time_since_epoch()
                    .count();
    seed %= std::numeric_limits<uint32_t>::max();
    m_shuffle.seed(static_cast<uint32_t>(seed));

    uint32_t locked_sz{};
    uint32_t spent_sz{};
    {
        std::uniform_int_distribution<uint32_t> locked(0, key_count);
        locked_sz = locked(m_shuffle);
        std::uniform_int_distribution<uint32_t> spent(0, key_count - locked_sz);
        spent_sz = spent(m_shuffle);
    }

    snapshot_map<hash_t, uhs_element> uhs = gen_map(key_count - (locked_sz + spent_sz));
    snapshot_map<hash_t, uhs_element> locked = gen_map(locked_sz);
    snapshot_map<hash_t, uhs_element> spent = gen_map(spent_sz, true);
    for(auto _ : state) {
        auto res = audit(uhs, locked, spent);
        ASSERT_NE(res, std::nullopt);
    }
}

BENCHMARK(audit_routine)
    ->RangeMultiplier(10)
    ->Range(10, SWEEP_MAX)
    ->Complexity(benchmark::oAuto);

BENCHMARK_MAIN();