// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util/network/socket_selector.hpp"
#include "util/network/tcp_listener.hpp"

#include <array>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

class SocketTest : public ::testing::Test {};

TEST_F(SocketTest, ListenConnectBasic) {
    auto listener = cbdc::network::tcp_listener();

    static constexpr auto portno = 29855;
    ASSERT_TRUE(listener.listen(cbdc::network::localhost, portno));

    auto conn_sock = cbdc::network::tcp_socket();
    std::thread conn_thread([&]() {
        const cbdc::network::endpoint_t ep{cbdc::network::localhost, portno};
        ASSERT_TRUE(conn_sock.connect(ep));
    });

    auto sock = cbdc::network::tcp_socket();
    ASSERT_TRUE(listener.accept(sock));

    conn_thread.join();

    auto pkt = cbdc::buffer();
    static constexpr auto pkt_sz = 32;
    std::array<unsigned char, pkt_sz> data{0, 1, 2, 3};
    pkt.append(data.data(), data.size());

    ASSERT_TRUE(conn_sock.send(pkt));

    auto recv_pkt = cbdc::buffer();
    ASSERT_TRUE(sock.receive(recv_pkt));
    ASSERT_EQ(recv_pkt, pkt);
}

TEST_F(SocketTest, selector_connect) {
    auto s = cbdc::network::socket_selector();
    ASSERT_TRUE(s.init());

    auto listener = cbdc::network::tcp_listener();
    static constexpr auto portno = 29855;
    ASSERT_TRUE(listener.listen(cbdc::network::localhost, portno));

    ASSERT_TRUE(s.add(listener));
    std::thread t([&]() {
        ASSERT_TRUE(s.wait());
    });

    auto sock = cbdc::network::tcp_socket();
    ASSERT_TRUE(sock.connect(cbdc::network::localhost, portno));

    t.join();
}

TEST_F(SocketTest, selector_unblock) {
    auto s = cbdc::network::socket_selector();
    ASSERT_TRUE(s.init());

    std::thread t([&]() {
        ASSERT_FALSE(s.wait());
    });

    s.unblock();

    t.join();

    auto listener = cbdc::network::tcp_listener();
    static constexpr auto portno = 29855;
    ASSERT_TRUE(listener.listen(cbdc::network::localhost, portno));

    ASSERT_TRUE(s.add(listener));
    t = std::thread([&]() {
        ASSERT_TRUE(s.wait());
    });

    auto sock = cbdc::network::tcp_socket();
    ASSERT_TRUE(sock.connect(cbdc::network::localhost, portno));

    t.join();

    auto client_sock = cbdc::network::tcp_socket();
    ASSERT_TRUE(listener.accept(client_sock));

    auto count = 0;
    auto unblock_called = false;
    auto mut = std::mutex();
    t = std::thread([&]() {
        for(;;) {
            {
                std::unique_lock<std::mutex> l(mut);
                if(unblock_called && count > 0) {
                    break;
                }
            }
            // Should unblock only once
            ASSERT_FALSE(s.wait());
            {
                std::unique_lock<std::mutex> l(mut);
                count++;
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    {
        std::lock_guard<std::mutex> l(mut);
        unblock_called = true;
    }
    s.unblock();

    t.join();

    {
        std::unique_lock<std::mutex> l(mut);
        ASSERT_EQ(count, 1);
    }
}
