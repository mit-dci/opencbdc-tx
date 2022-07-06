// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/transaction.hpp"
#include "util/common/hash.hpp"
#include "util/common/hashmap.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <leveldb/db.h>
#include <unordered_map>

class uhs_test : public ::testing::Test {
  protected:
    void SetUp() override {
        std::filesystem::remove_all(m_db_dir);
        leveldb::Options opt;
        opt.create_if_missing = true;

        leveldb::DB* db_ptr{};
        const auto res = leveldb::DB::Open(opt, m_db_dir, &db_ptr);
        ASSERT_TRUE(res.ok());
        this->m_db.reset(db_ptr);
    }

    void TearDown() override {
        std::filesystem::remove_all(m_db_dir);
    }

    std::unique_ptr<leveldb::DB> m_db;
    leveldb::ReadOptions m_read_options;
    leveldb::WriteOptions m_write_options;

    std::unordered_map<cbdc::hash_t,
                       cbdc::transaction::compact_output,
                       cbdc::hashing::null>
        m_proofs{};

    static constexpr const auto m_db_dir = "test_db";
};

TEST_F(uhs_test, leveldb_roundtrip) {
    cbdc::transaction::compact_output o{{'a', 'b', 'c', 'd'},
                                        {'e', 'f', 'g', 'h'},
                                        {'i', 'j', 'k', 'l'},
                                        {'m', 'n', 'o', 'p'}};

    std::array<char, 32> k;
    std::memcpy(k.data(), o.m_id.data(), k.size());
    leveldb::Slice Key(k.data(), k.size());

    std::array<char, sizeof(cbdc::transaction::compact_output)> v;
    auto* vptr = v.data();
    std::memcpy(vptr, o.m_id.data(), o.m_id.size());
    vptr += o.m_id.size();
    std::memcpy(vptr, o.m_auxiliary.data(), o.m_auxiliary.size());
    vptr += o.m_auxiliary.size();
    std::memcpy(vptr, o.m_range.data(), o.m_range.size());

    leveldb::Slice Val(v.data(), v.size());
    m_db->Put(this->m_write_options, Key, Val);

    std::string outval;
    const auto& res = m_db->Get(this->m_read_options, Key, &outval);
    EXPECT_FALSE(res.IsNotFound());
    EXPECT_EQ(std::memcmp(v.data(), outval.data(), v.size()), 0);
}

TEST_F(uhs_test, map_roundtrip) {
    cbdc::transaction::compact_output o{{'a', 'b', 'c', 'd'},
                                        {'e', 'f', 'g', 'h'},
                                        {'i', 'j', 'k', 'l'},
                                        {'m', 'n', 'o', 'p'}};

    m_proofs.emplace(o.m_id, o);
    const auto& p = m_proofs[o.m_id];
    EXPECT_EQ(p, o);
}
