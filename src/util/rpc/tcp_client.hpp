// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_TCP_CLIENT_H_
#define OPENCBDC_TX_SRC_RPC_TCP_CLIENT_H_

#include "client.hpp"
#include "util/common/variant_overloaded.hpp"
#include "util/network/connection_manager.hpp"

#include <future>
#include <unordered_map>

namespace cbdc::rpc {
    /// Implements an RPC client over TCP sockets. Accepts multiple server
    /// endpoints for failover purposes.
    /// \see cbdc::rpc::tcp_server
    /// \tparam Request type for requests.
    /// \tparam Response type for responses.
    template<typename Request, typename Response>
    class tcp_client : public client<Request, Response> {
      public:
        /// Constructor.
        /// \param server_endpoints RPC server endpoints to which to connect.
        explicit tcp_client(std::vector<network::endpoint_t> server_endpoints)
            : m_server_endpoints(std::move(server_endpoints)) {}

        tcp_client(tcp_client&&) = delete;
        auto operator=(tcp_client&&) -> tcp_client& = delete;
        tcp_client(const tcp_client&) = delete;
        auto operator=(const tcp_client&) -> tcp_client& = delete;

        using response_type =
            typename client<Request, Response>::response_type;

        /// Destructor. Disconnects from the RPC servers and stops the response
        /// handler thread.
        ~tcp_client() override {
            m_net.close();
            if(m_handler_thread.joinable()) {
                m_handler_thread.join();
            }
            {
                std::unique_lock<std::mutex> l(m_responses_mut);
                for(auto& [request_id, action] : m_responses) {
                    set_response_value(action, std::nullopt);
                }
                m_responses.clear();
            }
        }

        /// Initializes the client. Connects to the server endpoints and
        /// starts the response handler thread.
        /// \return true.
        [[nodiscard]] auto init() -> bool {
            if(!m_net.cluster_connect(m_server_endpoints, true)) {
                return false;
            }

            m_handler_thread = m_net.start_handler(
                [&](network::message_t&& msg) -> std::optional<buffer> {
                    return response_handler(std::move(msg));
                });

            return true;
        }

      private:
        network::connection_manager m_net;
        std::vector<network::endpoint_t> m_server_endpoints;
        std::thread m_handler_thread;

        using raw_callback_type =
            typename client<Request, Response>::raw_callback_type;

        using promise_type = std::promise<std::optional<response_type>>;
        using response_action_type
            = std::variant<promise_type, raw_callback_type>;

        std::mutex m_responses_mut;
        std::unordered_map<request_id_type, response_action_type> m_responses;

        auto send_request(cbdc::buffer request_buf,
                          request_id_type request_id,
                          response_action_type response_action) -> bool {
            {
                std::unique_lock<std::mutex> l(m_responses_mut);
                assert(m_responses.find(request_id) == m_responses.end());
                m_responses[request_id] = std::move(response_action);
            }
            auto pkt = std::make_shared<buffer>(std::move(request_buf));
            return m_net.send_to_one(pkt);
        }

        void set_response_value(response_action_type& response_action,
                                std::optional<response_type> value) {
            std::visit(overloaded{[&](promise_type& p) {
                                      p.set_value(std::move(value));
                                  },
                                  [&](raw_callback_type& cb) {
                                      cb(std::move(value));
                                  }},
                       response_action);
        }

        auto call_raw(cbdc::buffer request_buf,
                      request_id_type request_id,
                      std::chrono::milliseconds timeout)
            -> std::optional<response_type> override {
            auto response_promise = promise_type();
            auto response_future = response_promise.get_future();

            if(!send_request(std::move(request_buf),
                             request_id,
                             std::move(response_promise))) {
                set_response(request_id, std::nullopt);
                return std::nullopt;
            }

            if(timeout != std::chrono::milliseconds::zero()) {
                auto res = response_future.wait_for(timeout);
                if(res == std::future_status::timeout) {
                    set_response(request_id, std::nullopt);
                    return std::nullopt;
                }
            }

            return response_future.get();
        }

        auto response_handler(network::message_t&& msg)
            -> std::optional<buffer> {
            auto resp
                = client<Request, Response>::deserialize_response(*msg.m_pkt);
            if(resp.has_value()) {
                set_response(resp.value().m_header.m_request_id,
                             std::move(resp.value()));
            }
            return std::nullopt;
        }

        void set_response(request_id_type request_id,
                          std::optional<response_type> value) {
            auto response_node = [&]() {
                std::unique_lock<std::mutex> l(m_responses_mut);
                return m_responses.extract(request_id);
            }();

            if(!response_node.empty()) {
                set_response_value(response_node.mapped(), std::move(value));
            }
        }

        auto call_raw(cbdc::buffer request_buf,
                      request_id_type request_id,
                      raw_callback_type response_callback) -> bool override {
            if(!send_request(std::move(request_buf),
                             request_id,
                             std::move(response_callback))) {
                {
                    std::unique_lock<std::mutex> l(m_responses_mut);
                    m_responses.erase(request_id);
                }
                return false;
            }

            return true;
        }
    };
}

#endif
