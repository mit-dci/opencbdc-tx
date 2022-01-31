// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/transaction/messages.hpp"
#include "util.hpp"
#include "util/serialization/istream_serializer.hpp"
#include "util/serialization/ostream_serializer.hpp"

#include <filesystem>
#include <gtest/gtest.h>

class stream_serializer_test : public ::testing::Test {
  protected:
    void SetUp() override {
        m_of.open(m_test_file);
    }

    void TearDown() override {
        m_if.close();
        m_of.close();
        std::filesystem::remove_all(m_test_file);
    }

    std::ifstream m_if{};
    std::ofstream m_of{};

    cbdc::istream_serializer m_is{m_if};
    cbdc::ostream_serializer m_os{m_of};

    cbdc::test::compact_transaction m_tx{
        cbdc::test::simple_tx({'a', 'b', 'c'},
                              {{'d', 'e', 'f'}, {'g', 'h', 'i'}},
                              {{'x', 'y', 'z'}, {'z', 'z', 'z'}})};

    static constexpr auto m_test_file = "stream_test_file.dat";
};

TEST_F(stream_serializer_test, read_write) {
    uint64_t data{40};
    ASSERT_TRUE(m_os.write(&data, sizeof(data)));
    ASSERT_FALSE(m_os.read(&data, sizeof(data)));
    uint64_t rdata{};
    ASSERT_TRUE(m_if.good());
    m_of.close();
    ASSERT_EQ(std::filesystem::file_size(m_test_file), sizeof(data));
    m_if.open(m_test_file);
    ASSERT_TRUE(m_is.read(&rdata, sizeof(rdata)));
    ASSERT_FALSE(m_is.write(&rdata, sizeof(rdata)));
    ASSERT_EQ(rdata, data);
}

TEST_F(stream_serializer_test, eof) {
    ASSERT_TRUE(m_os.end_of_buffer());
}

TEST_F(stream_serializer_test, basic_roundtrip) {
    ASSERT_TRUE(m_os << m_tx);
    auto write_val = static_cast<uint64_t>(15);
    ASSERT_TRUE(m_os << write_val);

    ASSERT_TRUE(m_os.end_of_buffer());
    m_os.reset();
    ASSERT_FALSE(m_os.end_of_buffer());
    uint64_t no_read{};
    ASSERT_FALSE(m_os >> no_read);
    m_of.close();
    m_if.open(m_test_file);

    auto read_tx0 = cbdc::test::compact_transaction();
    ASSERT_TRUE(m_is >> read_tx0);
    ASSERT_EQ(read_tx0, m_tx);

    uint64_t yes_read{};
    ASSERT_TRUE(m_is >> yes_read);
    ASSERT_EQ(yes_read, write_val);
    ASSERT_TRUE(m_is.end_of_buffer());
    ASSERT_FALSE(m_is >> no_read);

    ASSERT_FALSE(m_is << yes_read);

    m_is.reset();
    ASSERT_EQ(m_if.tellg(), 0);
    ASSERT_FALSE(m_is.end_of_buffer());

    read_tx0 = cbdc::test::compact_transaction();
    ASSERT_TRUE(m_is >> read_tx0);
    ASSERT_EQ(read_tx0, m_tx);

    m_is.advance_cursor(2);
    ASSERT_FALSE(m_is >> no_read);
    ASSERT_TRUE(m_is.end_of_buffer());
    ASSERT_FALSE(m_is >> no_read);

    m_if.close();
    m_of.open(m_test_file);
    m_os.advance_cursor(2);
    ASSERT_TRUE(m_os << m_tx);
    m_os.reset();
    uint16_t w_val{20};
    ASSERT_TRUE(m_os << w_val);

    m_of.close();
    m_if.open(m_test_file);

    m_is.advance_cursor(2);
    read_tx0 = cbdc::test::compact_transaction();
    ASSERT_TRUE(m_is >> read_tx0);
    ASSERT_EQ(read_tx0, m_tx);

    m_is.reset();
    uint16_t r_val{};
    ASSERT_TRUE(m_is >> r_val);
    ASSERT_EQ(r_val, w_val);
}
