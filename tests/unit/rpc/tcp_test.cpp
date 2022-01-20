// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/async_server.hpp"
#include "rpc/blocking_server.hpp"
#include "rpc/tcp_client.hpp"
#include "rpc/tcp_server.hpp"
#include "serialization/format.hpp"

#include <gtest/gtest.h>
#include <variant>

TEST(tcp_rpc_test, echo_test) {
    using request = std::variant<bool, int>;
    using response = std::variant<int, bool>;

    auto ep = cbdc::network::endpoint_t{cbdc::network::localhost, 55555};
    auto server = cbdc::rpc::blocking_tcp_server<request, response>(ep);
    server.register_handler_callback(
        [](request req) -> std::optional<response> {
            auto resp = response{};
            std::visit(
                [&](auto val) {
                    resp = val;
                },
                req);
            return resp;
        });

    ASSERT_TRUE(server.init());

    auto client = cbdc::rpc::tcp_client<request, response>({ep});
    ASSERT_TRUE(client.init());

    auto req = request{true};
    auto resp = client.call(req);
    ASSERT_TRUE(resp.has_value());
    ASSERT_TRUE(std::holds_alternative<bool>(resp.value()));
    ASSERT_EQ(std::get<bool>(req), std::get<bool>(resp.value()));

    req = request{10};
    resp = client.call(req);
    ASSERT_TRUE(resp.has_value());
    ASSERT_TRUE(std::holds_alternative<int>(resp.value()));
    ASSERT_EQ(std::get<int>(req), std::get<int>(resp.value()));
}

TEST(tcp_rpc_test, response_error_test) {
    using request = int64_t;
    using response = int64_t;

    auto ep = cbdc::network::endpoint_t{cbdc::network::localhost, 55555};
    auto server = cbdc::rpc::blocking_tcp_server<request, response>(ep);
    server.register_handler_callback(
        [](request /* req */) -> std::optional<response> {
            return std::nullopt;
        });

    ASSERT_TRUE(server.init());

    auto client = cbdc::rpc::tcp_client<request, response>({ep});
    ASSERT_TRUE(client.init());

    auto req = request{0};
    auto resp = client.call(req);
    ASSERT_FALSE(resp.has_value());
}

TEST(tcp_rpc_test, timeout_test) {
    using request = int64_t;
    using response = int64_t;

    auto ep = cbdc::network::endpoint_t{cbdc::network::localhost, 55555};
    auto server = cbdc::rpc::blocking_tcp_server<request, response>(ep);
    server.register_handler_callback(
        [](request req) -> std::optional<response> {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            return req;
        });

    ASSERT_TRUE(server.init());

    auto client = cbdc::rpc::tcp_client<request, response>({ep});
    ASSERT_TRUE(client.init());

    auto req = request{10};
    auto resp = client.call(req, std::chrono::milliseconds(1));
    ASSERT_FALSE(resp.has_value());

    resp = client.call(req, std::chrono::milliseconds(1000));
    ASSERT_TRUE(resp.has_value());
    ASSERT_EQ(req, resp);
}

TEST(tcp_rpc_test, listen_fail_test) {
    using request = int64_t;
    using response = int64_t;

    auto ep = cbdc::network::endpoint_t{"8.8.8.8", 55555};
    auto server = cbdc::rpc::blocking_tcp_server<request, response>(ep);
    ASSERT_FALSE(server.init());
}

TEST(tcp_rpc_test, no_callback_test) {
    using request = int64_t;
    using response = int64_t;

    auto ep = cbdc::network::endpoint_t{cbdc::network::localhost, 55555};
    auto server = cbdc::rpc::blocking_tcp_server<request, response>(ep);
    ASSERT_TRUE(server.init());

    auto client = cbdc::rpc::tcp_client<request, response>({ep});
    ASSERT_TRUE(client.init());

    auto req = request{0};
    auto resp = client.call(req);
    ASSERT_FALSE(resp.has_value());
}

TEST(tcp_rpc_test, send_fail_test) {
    using request = int64_t;
    using response = int64_t;

    auto client = cbdc::rpc::tcp_client<request, response>(
        {{cbdc::network::localhost, 55555},
         {cbdc::network::localhost, 55556}});
    ASSERT_TRUE(client.init());

    auto req = request{0};
    auto resp = client.call(req);
    ASSERT_FALSE(resp.has_value());
}

TEST(tcp_rpc_test, cancel_test) {
    using request = int64_t;
    using response = int64_t;

    auto ep = cbdc::network::endpoint_t{cbdc::network::localhost, 55555};
    auto server = cbdc::network::tcp_listener();
    ASSERT_TRUE(server.listen(ep.first, ep.second));

    auto client = std::make_unique<cbdc::rpc::tcp_client<request, response>>(
        std::vector<cbdc::network::endpoint_t>{ep});
    ASSERT_TRUE(client->init());

    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        client.reset();
    });

    auto req = request{20};
    auto resp = client->call(req);

    ASSERT_FALSE(resp.has_value());

    t.join();
}

TEST(tcp_rpc_test, async_echo_test) {
    using request = std::variant<bool, int>;
    using response = std::variant<int, bool>;

    auto ep = cbdc::network::endpoint_t{cbdc::network::localhost, 55555};
    auto server = cbdc::rpc::async_tcp_server<request, response>(ep);
    server.register_handler_callback(
        [](request req,
           std::function<void(std::optional<response>)> cb) -> bool {
            std::thread([cb = std::move(cb), r = req]() {
                auto resp = response{};
                std::visit(
                    [&](auto val) {
                        resp = val;
                    },
                    r);
                cb(resp);
            }).detach();
            return true;
        });

    ASSERT_TRUE(server.init());

    auto client = cbdc::rpc::tcp_client<request, response>({ep});
    ASSERT_TRUE(client.init());

    auto done = std::promise<void>();
    auto done_fut = done.get_future();
    auto req = request{true};
    auto success = client.call(req, [&](std::optional<response> resp) {
        ASSERT_TRUE(resp.has_value());
        ASSERT_TRUE(std::holds_alternative<bool>(resp.value()));
        ASSERT_EQ(std::get<bool>(req), std::get<bool>(resp.value()));
        done.set_value();
    });
    ASSERT_TRUE(success);
    auto status = done_fut.wait_for(std::chrono::milliseconds(100));
    ASSERT_EQ(status, std::future_status::ready);

    done = std::promise<void>();
    done_fut = done.get_future();
    req = request{10};
    success = client.call(req, [&](std::optional<response> resp) {
        ASSERT_TRUE(resp.has_value());
        ASSERT_TRUE(std::holds_alternative<int>(resp.value()));
        ASSERT_EQ(std::get<int>(req), std::get<int>(resp.value()));
        done.set_value();
    });
    ASSERT_TRUE(success);
    status = done_fut.wait_for(std::chrono::milliseconds(100));
    ASSERT_EQ(status, std::future_status::ready);
}

TEST(tcp_rpc_test, async_error_test) {
    using request = bool;
    using response = bool;

    auto ep = cbdc::network::endpoint_t{cbdc::network::localhost, 55555};
    auto server = cbdc::rpc::async_tcp_server<request, response>(ep);
    server.register_handler_callback(
        [](request req,
           std::function<void(std::optional<response>)> cb) -> bool {
            if(req) {
                std::thread([cb = std::move(cb)]() {
                    cb(std::nullopt);
                }).detach();
            }
            return req;
        });

    ASSERT_TRUE(server.init());

    auto client = cbdc::rpc::tcp_client<request, response>({ep});
    ASSERT_TRUE(client.init());

    auto done = std::promise<void>();
    auto done_fut = done.get_future();
    auto req = true;
    auto success = client.call(req, [&](std::optional<response> resp) {
        ASSERT_FALSE(resp.has_value());
        done.set_value();
    });
    ASSERT_TRUE(success);
    auto status = done_fut.wait_for(std::chrono::milliseconds(100));
    ASSERT_EQ(status, std::future_status::ready);

    done = std::promise<void>();
    done_fut = done.get_future();
    req = true;
    success = client.call(req, [&](std::optional<response> resp) {
        ASSERT_FALSE(resp.has_value());
        done.set_value();
    });
    ASSERT_TRUE(success);
    status = done_fut.wait_for(std::chrono::milliseconds(100));
    ASSERT_EQ(status, std::future_status::ready);
}
