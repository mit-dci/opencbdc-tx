// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util/raft/serialization.hpp"
#include "util/serialization/buffer_serializer.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/size_serializer.hpp"

#include <gtest/gtest.h>
#include <utility>

class SerializationTest : public ::testing::Test {
  protected:
    SerializationTest() : ser(buf), deser(buf) {}

    void SetUp() override {
        ser.reset();
        deser.reset();
    }

    cbdc::buffer buf;
    cbdc::buffer_serializer ser;
    cbdc::buffer_serializer deser;
};

TEST_F(SerializationTest, TestIntegralPacket) {
    uint64_t val1{27};
    uint64_t val2{28};

    ser << val1 << val2;

    ASSERT_EQ(buf.size(), sizeof(val1) + sizeof(val2));

    uint64_t test_val1{};
    uint64_t test_val2{};

    deser >> test_val1 >> test_val2;

    ASSERT_EQ(val1, test_val1);
    ASSERT_EQ(val2, test_val2);
}

TEST_F(SerializationTest, TestIntegralNuraft) {
    auto arr = nuraft::buffer::alloc(sizeof(uint64_t) * 2);

    uint64_t val1{27};
    uint64_t val2{28};

    ser << val1 << val2;

    uint64_t test_val1{};
    uint64_t test_val2{};

    deser >> test_val1 >> test_val2;

    ASSERT_EQ(val1, test_val1);
    ASSERT_EQ(val2, test_val2);
}

TEST_F(SerializationTest, TestDummy) {
    uint32_t v0 = 0;
    uint64_t v1 = 2;
    ser << v0 << v1;

    auto sz = cbdc::size_serializer();
    sz << v0 << v1;
    ASSERT_EQ(buf.size(), sz.size());

    ASSERT_FALSE(sz.read(nullptr, 0));
    ASSERT_TRUE(sz);
    ASSERT_FALSE(sz.end_of_buffer());

    sz.reset();
    ASSERT_EQ(sz.size(), 0);

    sz.advance_cursor(10);
    ASSERT_EQ(sz.size(), 10);
}

TEST_F(SerializationTest, TestEndOfBuffer) {
    uint32_t v0 = 0;
    uint64_t v1 = 2;
    ser << v0 << v1;

    ser.reset();
    ASSERT_FALSE(ser.end_of_buffer());
    ser.advance_cursor(12);
    ASSERT_TRUE(ser.end_of_buffer());
}

TEST_F(SerializationTest, TestReadOutOfBounds) {
    uint32_t v0 = 0;
    uint64_t v1 = 2;
    ser << v0 << v1;

    deser.advance_cursor(12);
    ASSERT_FALSE(deser.read(nullptr, 10));
    ASSERT_FALSE(deser);
}
