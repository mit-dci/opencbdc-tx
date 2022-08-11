// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/twophase/locking_shard/format.hpp"
#include "util.hpp"

#include <gtest/gtest.h>

class locking_shard_format_test : public ::testing::Test {
  protected:
    cbdc::buffer m_target_packet{};
    cbdc::buffer_serializer m_ser{m_target_packet};
    cbdc::buffer_serializer m_deser{m_target_packet};

    cbdc::locking_shard::tx m_tx{};

    void SetUp() override {
        m_tx.m_tx.m_id = {'a', 'b', 'c'};
        m_tx.m_tx.m_inputs = {{'d', 'e', 'f'}, {'g', 'h', 'i'}};
        m_tx.m_tx.m_outputs = {{{'x'}, {'y'}, {'z'}}, {{'z'}, {'z'}, {'z'}}};
        m_tx.m_tx.m_attestations = {{{'a'}, {'b'}}};
    }
};

TEST_F(locking_shard_format_test, tx) {
    ASSERT_TRUE(m_ser << m_tx);

    auto deser_tx = cbdc::locking_shard::tx();
    ASSERT_TRUE(m_deser >> deser_tx);
    ASSERT_EQ(m_tx, deser_tx);
}

TEST_F(locking_shard_format_test, lock_request) {
    auto req = cbdc::locking_shard::rpc::request();
    req.m_dtx_id = {'b'};
    req.m_params = cbdc::locking_shard::rpc::lock_params({m_tx, m_tx});
    ASSERT_TRUE(m_ser << req);

    auto deser_req = cbdc::locking_shard::rpc::request();
    ASSERT_TRUE(m_deser >> deser_req);
    ASSERT_EQ(req, deser_req);
}

TEST_F(locking_shard_format_test, apply_request) {
    auto req = cbdc::locking_shard::rpc::request();
    req.m_dtx_id = {'b'};
    req.m_params = cbdc::locking_shard::rpc::apply_params({true, false});
    ASSERT_TRUE(m_ser << req);

    auto deser_req = cbdc::locking_shard::rpc::request();
    ASSERT_TRUE(m_deser >> deser_req);
    ASSERT_EQ(req, deser_req);
}

TEST_F(locking_shard_format_test, discard_request) {
    auto req = cbdc::locking_shard::rpc::request();
    req.m_dtx_id = {'b'};
    req.m_params = cbdc::locking_shard::rpc::discard_params();
    ASSERT_TRUE(m_ser << req);

    auto deser_req = cbdc::locking_shard::rpc::request();
    ASSERT_TRUE(m_deser >> deser_req);
    ASSERT_EQ(req, deser_req);
}

TEST_F(locking_shard_format_test, lock_response) {
    auto req = cbdc::locking_shard::rpc::response();
    req = cbdc::locking_shard::rpc::lock_response({true, false});
    ASSERT_TRUE(m_ser << req);

    auto deser_req = cbdc::locking_shard::rpc::response();
    ASSERT_TRUE(m_deser >> deser_req);
    ASSERT_EQ(req, deser_req);
}

TEST_F(locking_shard_format_test, apply_response) {
    auto req = cbdc::locking_shard::rpc::response();
    req = cbdc::locking_shard::rpc::apply_response();
    ASSERT_TRUE(m_ser << req);

    auto deser_req = cbdc::locking_shard::rpc::response();
    ASSERT_TRUE(m_deser >> deser_req);
    ASSERT_EQ(req, deser_req);
}

TEST_F(locking_shard_format_test, discard_response) {
    auto req = cbdc::locking_shard::rpc::response();
    req = cbdc::locking_shard::rpc::discard_response();
    ASSERT_TRUE(m_ser << req);

    auto deser_req = cbdc::locking_shard::rpc::response();
    ASSERT_TRUE(m_deser >> deser_req);
    ASSERT_EQ(req, deser_req);
}
