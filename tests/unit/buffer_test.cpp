// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util/common/buffer.hpp"

#include <cstring>
#include <gtest/gtest.h>

TEST(BufferTest, to_from_hex) {
    std::string data = "hello";

    auto buf = cbdc::buffer();
    buf.extend(data.size());
    std::memcpy(buf.data(), data.data(), data.size());
    auto hex = buf.to_hex();
    ASSERT_EQ(hex, "68656c6c6f");

    auto from = cbdc::buffer::from_hex(hex);
    ASSERT_EQ(from.value(), buf);
}

TEST(BufferTest, from_hex_invalid_char) {
    std::string data = "ZZ11ff";

    auto buf = cbdc::buffer();
    buf.extend(data.size());
    std::memcpy(buf.data(), data.data(), data.size());

    auto from = cbdc::buffer::from_hex(data);
    ASSERT_EQ(from.has_value(), false);
}

TEST(BufferTest, from_hex_invalid_len) {
    std::string data = "11ffa";

    auto buf = cbdc::buffer();
    buf.extend(data.size());
    std::memcpy(buf.data(), data.data(), data.size());

    auto from = cbdc::buffer::from_hex(data);
    ASSERT_EQ(from.has_value(), false);
}

TEST(BufferTest, from_hex_empty) {
    std::string data;

    auto buf = cbdc::buffer();
    buf.extend(data.size());
    std::memcpy(buf.data(), data.data(), data.size());

    auto from = cbdc::buffer::from_hex(data);
    ASSERT_EQ(from.has_value(), false);
}
