// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coordinator/distributed_tx.hpp"
#include "locking_shard/locking_shard.hpp"

#include <gtest/gtest.h>
#include <queue>
#include <random>

class TwoPhaseTest : public ::testing::Test {};

TEST_F(TwoPhaseTest, test_one_shard) {
    auto logger = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::debug);
    auto shard = cbdc::locking_shard::locking_shard(std::make_pair(0, 255),
                                                    logger,
                                                    10000000,
                                                    "");

    auto txs = std::vector<cbdc::locking_shard::tx>();
    for(size_t i{0}; i < 1000; i++) {
        auto tx = cbdc::locking_shard::tx();
        auto uhs_id = cbdc::hash_t();
        std::memcpy(uhs_id.data(), &i, sizeof(i));
        tx.m_creating.push_back(uhs_id);
        txs.push_back(tx);
    }

    auto tx_size = txs.size();
    auto lock_res = shard.lock_outputs(std::move(txs), cbdc::hash_t());
    ASSERT_EQ(lock_res->size(), tx_size);
    for(auto r : *lock_res) {
        ASSERT_TRUE(r);
    }

    shard.apply_outputs(std::move(*lock_res), cbdc::hash_t());
}

TEST_F(TwoPhaseTest, test_two_shards) {
    auto logger = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::debug);
    auto shard0 = std::make_shared<cbdc::locking_shard::locking_shard>(
        std::make_pair(0, 127),
        logger,
        10000000,
        "");
    auto shard1 = std::make_shared<cbdc::locking_shard::locking_shard>(
        std::make_pair(128, 255),
        logger,
        10000000,
        "");
    auto shards = std::vector<std::shared_ptr<cbdc::locking_shard::interface>>(
        {shard0, shard1});

    auto txs = std::vector<cbdc::transaction::compact_tx>();
    for(size_t i{0}; i < 1000; i++) {
        auto tx = cbdc::transaction::compact_tx();
        std::memcpy(tx.m_id.data(), &i, sizeof(i));
        auto uhs_id = cbdc::hash_t();
        std::memcpy(uhs_id.data(), &i, sizeof(i));
        tx.m_uhs_outputs.push_back(uhs_id);
        txs.push_back(tx);
    }

    auto coordinator
        = cbdc::coordinator::distributed_tx(cbdc::hash_t(), shards, logger);
    for(const auto& tx : txs) {
        coordinator.add_tx(tx);
    }
    auto res = coordinator.execute();
    ASSERT_EQ(res->size(), txs.size());
    for(auto r : *res) {
        ASSERT_TRUE(r);
    }
}

TEST_F(TwoPhaseTest, test_one_shard_random) {
    auto logger = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::debug);
    auto shard = cbdc::locking_shard::locking_shard(std::make_pair(0, 255),
                                                    logger,
                                                    10000000,
                                                    "");

    auto e = std::default_random_engine();
    auto rnd = std::uniform_int_distribution<uint64_t>();

    auto outputs = std::queue<cbdc::hash_t>();

    auto txs = std::vector<cbdc::locking_shard::tx>();
    for(size_t i{0}; i < 1000; i++) {
        auto tx = cbdc::locking_shard::tx();
        auto output0 = cbdc::hash_t();
        auto output1 = cbdc::hash_t();
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&output0[j * 8], &val, sizeof(val));
        }
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&output1[j * 8], &val, sizeof(val));
        }
        tx.m_creating.push_back(output0);
        tx.m_creating.push_back(output1);
        outputs.push(output0);
        outputs.push(output1);
        txs.push_back(tx);
    }

    auto tx_size = txs.size();
    auto lock_res = shard.lock_outputs(std::move(txs), cbdc::hash_t());
    ASSERT_EQ(lock_res->size(), tx_size);
    for(auto r : *lock_res) {
        ASSERT_TRUE(r);
    }

    shard.apply_outputs(std::move(*lock_res), cbdc::hash_t());

    txs = std::vector<cbdc::locking_shard::tx>();
    for(size_t i{0}; i < 1000; i++) {
        auto tx = cbdc::locking_shard::tx();
        auto output0 = cbdc::hash_t();
        auto output1 = cbdc::hash_t();
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&output0[j * 8], &val, sizeof(val));
        }
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&output1[j * 8], &val, sizeof(val));
        }
        tx.m_creating.push_back(output0);
        tx.m_creating.push_back(output1);
        tx.m_spending.push_back(outputs.front());
        outputs.pop();
        tx.m_spending.push_back(outputs.front());
        outputs.pop();
        txs.push_back(tx);
    }

    tx_size = txs.size();
    lock_res = shard.lock_outputs(std::move(txs), cbdc::hash_t());
    ASSERT_EQ(lock_res->size(), tx_size);
    for(auto r : *lock_res) {
        ASSERT_TRUE(r);
    }

    shard.apply_outputs(std::move(*lock_res), cbdc::hash_t());
}

TEST_F(TwoPhaseTest, test_two_shards_random) {
    auto logger = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::debug);
    auto shard0 = std::make_shared<cbdc::locking_shard::locking_shard>(
        std::make_pair(0, 127),
        logger,
        10000000,
        "");
    auto shard1 = std::make_shared<cbdc::locking_shard::locking_shard>(
        std::make_pair(128, 255),
        logger,
        10000000,
        "");
    auto shards = std::vector<std::shared_ptr<cbdc::locking_shard::interface>>(
        {shard0, shard1});

    auto e = std::default_random_engine();
    auto rnd = std::uniform_int_distribution<uint64_t>();

    auto outputs = std::queue<cbdc::hash_t>();

    auto txs = std::vector<cbdc::transaction::compact_tx>();
    for(size_t i{0}; i < 1000; i++) {
        auto tx = cbdc::transaction::compact_tx();
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&tx.m_id[j * 8], &val, sizeof(val));
        }
        auto output0 = cbdc::hash_t();
        auto output1 = cbdc::hash_t();
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&output0[j * 8], &val, sizeof(val));
        }
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&output1[j * 8], &val, sizeof(val));
        }
        tx.m_uhs_outputs.push_back(output0);
        tx.m_uhs_outputs.push_back(output1);
        outputs.push(output0);
        outputs.push(output1);
        txs.push_back(tx);
    }

    auto coordinator
        = cbdc::coordinator::distributed_tx(cbdc::hash_t(), shards, logger);
    for(const auto& tx : txs) {
        coordinator.add_tx(tx);
    }
    auto res = coordinator.execute();
    ASSERT_EQ(res->size(), txs.size());
    for(auto r : *res) {
        ASSERT_TRUE(r);
    }

    txs = std::vector<cbdc::transaction::compact_tx>();
    for(size_t i{0}; i < 1000; i++) {
        auto tx = cbdc::transaction::compact_tx();
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&tx.m_id[j * 8], &val, sizeof(val));
        }
        auto output0 = cbdc::hash_t();
        auto output1 = cbdc::hash_t();
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&output0[j * 8], &val, sizeof(val));
        }
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&output1[j * 8], &val, sizeof(val));
        }
        tx.m_uhs_outputs.push_back(output0);
        tx.m_uhs_outputs.push_back(output1);
        tx.m_inputs.push_back(outputs.front());
        outputs.pop();
        tx.m_inputs.push_back(outputs.front());
        outputs.pop();
        txs.push_back(tx);
    }

    auto coordinator2
        = cbdc::coordinator::distributed_tx(cbdc::hash_t(), shards, logger);
    for(const auto& tx : txs) {
        coordinator2.add_tx(tx);
    }
    res = coordinator2.execute();
    ASSERT_EQ(res->size(), txs.size());
    for(auto r : *res) {
        ASSERT_TRUE(r);
    }
}

TEST_F(TwoPhaseTest, test_two_shards_conflicting) {
    auto logger = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::debug);
    auto shard0 = std::make_shared<cbdc::locking_shard::locking_shard>(
        std::make_pair(0, 127),
        logger,
        10000000,
        "");
    auto shard1 = std::make_shared<cbdc::locking_shard::locking_shard>(
        std::make_pair(128, 255),
        logger,
        10000000,
        "");
    auto shards = std::vector<std::shared_ptr<cbdc::locking_shard::interface>>(
        {shard0, shard1});

    auto e = std::default_random_engine();
    auto rnd = std::uniform_int_distribution<uint64_t>();

    auto outputs = std::queue<cbdc::hash_t>();

    auto txs = std::vector<cbdc::transaction::compact_tx>();
    for(size_t i{0}; i < 1000; i++) {
        auto tx = cbdc::transaction::compact_tx();
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&tx.m_id[j * 8], &val, sizeof(val));
        }
        auto output0 = cbdc::hash_t();
        auto output1 = cbdc::hash_t();
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&output0[j * 8], &val, sizeof(val));
        }
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&output1[j * 8], &val, sizeof(val));
        }
        tx.m_uhs_outputs.push_back(output0);
        tx.m_uhs_outputs.push_back(output1);
        outputs.push(output0);
        outputs.push(output1);
        txs.push_back(tx);
    }

    auto coordinator
        = cbdc::coordinator::distributed_tx(cbdc::hash_t(), shards, logger);
    for(const auto& tx : txs) {
        coordinator.add_tx(tx);
    }
    auto res = coordinator.execute();
    ASSERT_EQ(res->size(), txs.size());
    for(auto r : *res) {
        ASSERT_TRUE(r);
    }
    for(const auto& tx : txs) {
        auto res0 = *shard0->check_tx_id(tx.m_id);
        auto res1 = *shard1->check_tx_id(tx.m_id);
        ASSERT_TRUE((res0 || res1) && (res0 ^ res1));
    }

    txs = std::vector<cbdc::transaction::compact_tx>();
    for(size_t i{0}; i < 1000; i++) {
        auto tx = cbdc::transaction::compact_tx();
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&tx.m_id[j * 8], &val, sizeof(val));
        }
        auto output0 = cbdc::hash_t();
        auto output1 = cbdc::hash_t();
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&output0[j * 8], &val, sizeof(val));
        }
        for(size_t j{0}; j < 4; j++) {
            const auto val = rnd(e);
            std::memcpy(&output1[j * 8], &val, sizeof(val));
        }
        tx.m_uhs_outputs.push_back(output0);
        tx.m_uhs_outputs.push_back(output1);
        tx.m_inputs.push_back(outputs.front());
        outputs.pop();
        tx.m_inputs.push_back(outputs.front());
        outputs.pop();
        txs.push_back(tx);
    }

    auto coordinator2
        = cbdc::coordinator::distributed_tx(cbdc::hash_t(), shards, logger);
    for(const auto& tx : txs) {
        coordinator2.add_tx(tx);
    }
    for(const auto& tx : txs) {
        coordinator2.add_tx(tx);
    }
    res = coordinator2.execute();
    ASSERT_EQ(res->size(), 2 * txs.size());
    for(size_t i{0}; i < txs.size(); i++) {
        ASSERT_TRUE((*res)[i]);
        auto& tx = txs[i];
        for(const auto& out : tx.m_uhs_outputs) {
            auto res0 = *shard0->check_unspent(out);
            auto res1 = *shard1->check_unspent(out);
            ASSERT_TRUE((res0 || res1) && (res0 ^ res1));
        }
        for(const auto& inp : tx.m_inputs) {
            auto res0 = *shard0->check_unspent(inp);
            auto res1 = *shard1->check_unspent(inp);
            ASSERT_FALSE(res0 || res1);
        }
        auto res0 = *shard0->check_tx_id(tx.m_id);
        auto res1 = *shard1->check_tx_id(tx.m_id);
        ASSERT_TRUE((res0 || res1) && (res0 ^ res1));
    }
    for(size_t i{txs.size()}; i < 2 * txs.size(); i++) {
        ASSERT_FALSE((*res)[i]);
    }
}
