// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.hpp"
#include "util/network/connection_manager.hpp"
#include "util/serialization/buffer_serializer.hpp"

#include <gtest/gtest.h>

class NetworkTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_blocking_net
            = std::make_unique<decltype(m_blocking_net)::element_type>();
    }

    std::unique_ptr<cbdc::network::connection_manager> m_blocking_net;
};

/// Tests that the send template function of cbdc::network::network compiles
/// correctly
TEST_F(NetworkTest, send_template) {
    static constexpr auto listen_port = 30001;
    ASSERT_TRUE(m_blocking_net->listen(cbdc::network::localhost, listen_port));

    std::thread listen_thr([&]() {
        ASSERT_TRUE(m_blocking_net->pump());
    });

    auto sock = std::make_unique<cbdc::network::tcp_socket>();
    ASSERT_TRUE(sock->connect(cbdc::network::localhost, listen_port));

    auto client_net = cbdc::network::connection_manager();

    auto peer_id = client_net.add(std::move(sock));
    auto pkt = std::make_shared<cbdc::buffer>();
    client_net.send(pkt, peer_id);

    const auto pkts = m_blocking_net->handle_messages();
    ASSERT_EQ(pkts.size(), size_t{1});
    for(const auto& p : pkts) {
        ASSERT_TRUE(p.m_pkt);
    }

    m_blocking_net->close();
    client_net.close();
    listen_thr.join();
}

TEST_F(NetworkTest, invalid_connection) {
    cbdc::network::endpoint_t sever_ep{cbdc::network::localhost, 30002};
    auto client = m_blocking_net->start_cluster_handler(
        {sever_ep},
        [](cbdc::network::message_t&& /* unused */)
            -> std::optional<cbdc::buffer> {
            return std::nullopt;
        });
    ASSERT_FALSE(client.has_value());
}

TEST_F(NetworkTest, client_server) {
    cbdc::network::endpoint_t sever_ep{cbdc::network::localhost, 30002};
    auto server = m_blocking_net->start_server(
        sever_ep,
        [](cbdc::network::message_t&& pkt) -> std::optional<cbdc::buffer> {
            uint32_t req{};
            auto deser = cbdc::buffer_serializer(*pkt.m_pkt);
            deser >> req;
            cbdc::buffer res{};
            auto ser = cbdc::buffer_serializer(res);
            ser << (req * 2);
            return res;
        });

    ASSERT_TRUE(server.has_value());

    auto sc = cbdc::test::simple_client<uint32_t>();
    ASSERT_TRUE(sc.connect({sever_ep}));
    ASSERT_EQ(sc.get(22), 44);

    m_blocking_net->close();
    server->join();
}

TEST_F(NetworkTest, reset_net) {
    static constexpr auto listen_port = 30001;
    ASSERT_TRUE(m_blocking_net->listen(cbdc::network::localhost, listen_port));
    auto listener = m_blocking_net->start_server_listener();

    auto sock = std::make_unique<cbdc::network::tcp_socket>();
    ASSERT_TRUE(sock->connect(cbdc::network::localhost, listen_port));

    m_blocking_net->close();
    listener.join();
    ASSERT_EQ(m_blocking_net->peer_count(), 0UL);

    m_blocking_net->reset();
    ASSERT_TRUE(m_blocking_net->listen(cbdc::network::localhost, listen_port));
    listener = m_blocking_net->start_server_listener();

    sock = std::make_unique<cbdc::network::tcp_socket>();
    ASSERT_TRUE(sock->connect(cbdc::network::localhost, listen_port));

    m_blocking_net->close();
    listener.join();
}
