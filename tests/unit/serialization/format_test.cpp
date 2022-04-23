// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util/raft/serialization.hpp"
#include "util/serialization/buffer_serializer.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/size_serializer.hpp"
#include "uhs/atomizer/watchtower/watchtower.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <utility>

class format_test : public ::testing::Test {
  protected:
    format_test() : ser(buf), deser(buf) {}

    void SetUp() override {
        ser.reset();
        deser.reset();
    }

    cbdc::buffer buf;
    cbdc::buffer_serializer ser;
    cbdc::buffer_serializer deser;
};

TEST_F(format_test, inordinate_declared_lengths_are_handled) {
    // manually serialize a vector declaring an obscenely large size
    ser << std::numeric_limits<uint64_t>::max();
    ser << 12LLU;
    ser << 75LLU;
    ser << std::numeric_limits<uint64_t>::max();
    ser << 37LLU;

    EXPECT_TRUE(ser);

    std::vector<uint64_t> r0;
    deser >> r0;

    EXPECT_FALSE(deser);
    EXPECT_EQ(r0.size(), 4);
    EXPECT_EQ(r0.capacity(), 1024UL * 1024UL / sizeof(uint64_t));
}

TEST_F(format_test, wellformed_optionals_roundtrip) {
    // Nothing
    std::optional<uint64_t> o0(std::nullopt);
    ser << o0;
    EXPECT_TRUE(ser);

    std::optional<uint64_t> r{};
    deser >> r;

    EXPECT_TRUE(deser);
    EXPECT_EQ(r.has_value(), false);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    // Just 17
    std::optional<uint64_t> o1{17};
    ser << o1;
    EXPECT_TRUE(ser);

    deser >> r;

    EXPECT_TRUE(deser);
    EXPECT_EQ(r.value(), o1.value());
}

TEST_F(format_test, malformed_optionals_cannot_roundtrip) {
    std::optional<uint64_t> r{};
    deser >> r;

    // read from the buffer before anything was written
    EXPECT_FALSE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    // specify that there is a value when there isn't one
    ser << true;
    deser >> r;

    // read a value that doesn't exist
    EXPECT_FALSE(deser);
}

TEST_F(format_test, wellformed_pairs_roundtrip) {
    std::pair<uint64_t, bool> p{27, false};
    ser << p;
    EXPECT_TRUE(ser);

    std::pair<uint64_t, bool> result{};
    deser >> result;

    EXPECT_TRUE(deser);
    EXPECT_EQ(p.first, result.first);
    EXPECT_EQ(p.second, result.second);
}

TEST_F(format_test, malformed_pairs_cannot_roundtrip) {
    // incorrect type (fst will fail to parse);
    ser << true;
    EXPECT_TRUE(ser);

    std::pair<uint64_t, bool> r1{};
    deser >> r1;

    EXPECT_FALSE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    // not all values necessary (snd will fail to parse)
    ser << 47LLU;

    std::pair<uint64_t, bool> r2{};
    deser >> r2;

    EXPECT_FALSE(deser);
}

TEST_F(format_test, wellformed_vectors_roundtrip) {
    std::vector<uint64_t> v0{};

    // empty vector
    ser << v0;
    EXPECT_TRUE(ser);

    std::vector<uint64_t> r0{};
    deser >> r0;

    EXPECT_TRUE(deser);
    EXPECT_EQ(r0.size(), v0.size());
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    std::vector<uint64_t> v1{0, std::numeric_limits<uint64_t>::max(), 75};

    // random test vector
    ser << v1;
    EXPECT_TRUE(ser);

    std::vector<uint64_t> result{};
    deser >> result;

    EXPECT_TRUE(deser);
    EXPECT_EQ(v1.size(), result.size());
    for(uint64_t i = 0; i < v1.size(); ++i) {
        EXPECT_EQ(v1[i], result[i]);
    }
}

TEST_F(format_test, malformed_vectors_cannot_roundtrip) {
    std::vector<uint64_t> r0{};

    // read from buffer without anything present (fails on reading length)
    deser >> r0;
    EXPECT_FALSE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    std::vector<uint64_t> v1{75, std::numeric_limits<uint64_t>::max(), 0};
    size_t sz = v1.size();

    // manually serialize a vector, specifying an inaccurate size
    ser << sz + 5;
    EXPECT_TRUE(ser);
    for(uint64_t i = 0; i < sz; ++i) {
        ser << v1[i];
        EXPECT_TRUE(ser);
    }

    std::vector<uint64_t> r1{};
    deser >> r1;

    // attempt to parse too many elements
    EXPECT_FALSE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    std::vector<bool> v2{true, false, true, true, false};
    ser << v2;
    EXPECT_TRUE(ser);

    std::vector<uint64_t> r2{};
    deser >> r2;

    // attempt to reinterpret bytes
    EXPECT_FALSE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    std::vector<uint64_t> v3{12,
                             7,
                             5000009,
                             std::numeric_limits<uint32_t>::max(),
                             0};

    // manually serialize a vector, not specifying a size
    for(uint64_t i : v3) {
        ser << i;
        EXPECT_TRUE(ser);
    }

    // equivalent, rather than having four bytes more for the length
    EXPECT_EQ(buf.size(), v3.size() * sizeof(uint64_t));

    std::vector<uint64_t> r3{};
    deser >> r3;

    EXPECT_NE(r3.size(), sz);
    EXPECT_FALSE(deser);
}

TEST_F(format_test, wellformed_unordered_maps_roundtrip) {
    std::unordered_map<int16_t, uint64_t> m0{};
    ser << m0;
    EXPECT_TRUE(ser);

    std::unordered_map<int16_t, uint64_t> r0{};
    deser >> r0;

    // empty map
    EXPECT_TRUE(deser);
    EXPECT_EQ(r0.size(), m0.size());
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    m0.emplace(std::make_pair(0, std::numeric_limits<uint64_t>::max()));
    m0.emplace(std::make_pair(-1, 0));
    m0.emplace(std::make_pair(std::numeric_limits<int16_t>::min(), 1 << 7));

    ser << m0;
    EXPECT_TRUE(ser);

    std::unordered_map<int16_t, uint64_t> r1{};
    deser >> r1;

    // random map
    EXPECT_TRUE(deser);
    for(const auto& [k, v] : r1) {
        EXPECT_EQ(v, m0[k]);
    }
}

TEST_F(format_test, malformed_unordered_maps_cannot_roundtrip) {
    std::unordered_map<int16_t, uint64_t> r0{};
    deser >> r0;

    // attempt to read without anything in the buffer (fails on reading length)
    EXPECT_FALSE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    // say there is a key-value pair when there is none
    ser << 1LLU;
    std::unordered_map<int16_t, uint64_t> r1{};
    deser >> r1;

    // fails at attempting to read non-existant key
    EXPECT_FALSE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    std::unordered_map<int16_t, uint64_t> m1{
        {0, std::numeric_limits<uint64_t>::max()},
        {-1, 0},
        {std::numeric_limits<int16_t>::min(), 1 << 7}};

    // manually serialize map, where the last key-value has no value
    ser << m1.size() + 1;
    for(const auto& [k, v] : m1) {
        ser << k << v;
        EXPECT_TRUE(ser);
    }
    ser << int16_t(45);
    EXPECT_TRUE(ser);

    std::unordered_map<int16_t, uint64_t> r2{};
    deser >> r2;

    // fails at trying to read the last value
    EXPECT_FALSE(deser);
    EXPECT_EQ(m1.size(), r2.size());
}

TEST_F(format_test, wellformed_sets_roundtrip) {
    // empty set
    std::set<uint64_t> s0;
    ser << s0;

    std::set<uint64_t> r0;
    deser >> r0;

    EXPECT_TRUE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    // random set
    std::set<uint64_t> s1;
    s1.insert(0);
    s1.insert(std::numeric_limits<uint64_t>::max());
    s1.insert(1 << 13);

    ser << s1;

    std::set<uint64_t> r1;
    deser >> r1;

    EXPECT_TRUE(deser);
    for(const auto& k : r1) {
        EXPECT_TRUE(s1.find(k) != s1.end());
    }
}

TEST_F(format_test, malformed_sets_cannot_roundtrip) {
    // attempt to read without anything in the buffer (fails on reading length)
    std::set<uint64_t> r0;
    deser >> r0;

    EXPECT_FALSE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    std::set<uint64_t> s0;
    s0.insert(0);
    s0.insert(std::numeric_limits<uint64_t>::max());
    s0.insert(1 << 13);

    // manually serialize a set specifying an incorrect length
    ser << s0.size() + 5;
    for(const auto& k : s0) {
        ser << k;
        EXPECT_TRUE(ser);
    }

    std::set<uint64_t> r1;
    deser >> r1;

    // attempt to read more items than exist (fails at reading non-existent
    // member)
    EXPECT_FALSE(deser);
}

TEST_F(format_test, wellformed_unordered_sets_roundtrip) {
    // empty set
    std::unordered_set<uint64_t> s0;
    ser << s0;

    std::unordered_set<uint64_t> r0;
    deser >> r0;

    EXPECT_TRUE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    // random set
    std::unordered_set<uint64_t> s1;
    s1.insert(0);
    s1.insert(std::numeric_limits<uint64_t>::max());
    s1.insert(1 << 13);

    ser << s1;

    std::unordered_set<uint64_t> r1;
    deser >> r1;

    EXPECT_TRUE(deser);
    for(const auto& k : r1) {
        EXPECT_TRUE(s1.find(k) != s1.end());
    }
}

TEST_F(format_test, malformed_unordered_sets_cannot_roundtrip) {
    // attempt to read without anything in the buffer (fails on reading length)
    std::unordered_set<uint64_t> r0;
    deser >> r0;

    EXPECT_FALSE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);

    std::unordered_set<uint64_t> s0;
    s0.insert(0);
    s0.insert(std::numeric_limits<uint64_t>::max());
    s0.insert(1 << 13);

    // manually serialize a set specifying an incorrect length
    ser << s0.size() + 5;
    for(const auto& k : s0) {
        ser << k;
        EXPECT_TRUE(ser);
    }

    std::unordered_set<uint64_t> r1;
    deser >> r1;

    // attempt to read more items than exist (fails at reading non-existent
    // member)
    EXPECT_FALSE(deser);
}

TEST_F(format_test, get_variant_default_constructibles) {
    // empty set
    using T = typename std::unordered_set<uint64_t>;
    using V = typename std::variant<T, uint64_t>;
    T s0;
    V v0{s0};
    // how ser and deser are connected?
    // my answer is the variant
    ser << v0;
    auto r0 = cbdc::get_variant<T, uint64_t>(deser);
    EXPECT_TRUE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);
    EXPECT_TRUE(std::holds_alternative<T>(r0));

    // non empty
    T s1;
    s1.insert(0);
    s1.insert(std::numeric_limits<uint64_t>::max());
    s1.insert(1 << 13);
    V v1{s1};
    ser << v1;
    auto r1 = cbdc::get_variant<T, uint64_t>(deser);
    EXPECT_TRUE(std::holds_alternative<T>(r1));
    auto resulted_set = std::get<T>(r1);
    EXPECT_TRUE(deser);
    for(const auto& k : resulted_set) {
        EXPECT_TRUE(s1.find(k) != s1.end());
    }
}
//
TEST_F(format_test, get_variant_nondefault_constructibles) {
    using R = typename cbdc::watchtower::request;
    using H = typename cbdc::watchtower::best_block_height_response;
    using V = typename std::variant<R, H>;
    H block_height_0{9};
    V variant_0{block_height_0};
    ser << variant_0;
    auto r0 = cbdc::get_variant<R, H>(deser);
    EXPECT_TRUE(deser);
    ser.reset();
    deser.reset();
    EXPECT_TRUE(ser);
    EXPECT_TRUE(std::holds_alternative<H>(r0));
    EXPECT_TRUE(std::get<H>(r0).height() == block_height_0.height());
}
