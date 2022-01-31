// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_TCP_SERVER_H_
#define OPENCBDC_TX_SRC_RPC_TCP_SERVER_H_

#include "async_server.hpp"
#include "blocking_server.hpp"
#include "server.hpp"
#include "util/network/connection_manager.hpp"

namespace cbdc::rpc {
    /// Implements an RPC server over a TCP socket.
    /// \see cbdc::rpc::tcp_client
    /// \see cbdc::rpc::server
    /// \tparam Server type implementing request handling.
    template<typename Server>
    class tcp_server : public Server {
      public:
        /// Constructor.
        /// \param listen_endpoint endpoint on which to listen for incoming connections.
        explicit tcp_server(network::endpoint_t listen_endpoint)
            : m_net(std::make_shared<network::connection_manager>()),
              m_listen_endpoint(std::move(listen_endpoint)) {}

        tcp_server(tcp_server&&) = delete;
        auto operator=(tcp_server&&) -> tcp_server& = delete;
        tcp_server(const tcp_server&) = delete;
        auto operator=(const tcp_server&) -> tcp_server& = delete;

        /// Destructor. Closes the listener and stops the message handler
        /// thread.
        ~tcp_server() override {
            m_net->close();
            if(m_handler_thread.joinable()) {
                m_handler_thread.join();
            }
        }

        /// Initializes the server. Starts listening on the server endpoint and
        /// starts the message handler thread.
        /// \return false if the server was unable to bind to the server
        ///         endpoint.
        [[nodiscard]] auto init() -> bool {
            auto handler_thread = m_net->start_server(
                m_listen_endpoint,
                [&](network::message_t&& msg) -> std::optional<cbdc::buffer> {
                    auto ret = std::optional<cbdc::buffer>();
                    if constexpr(Server::handler == handler_type::async) {
                        ret = Server::async_call(
                            std::move(*msg.m_pkt),
                            [&, peer_id = msg.m_peer_id, net = m_net](
                                cbdc::buffer resp) {
                                auto resp_ptr = std::make_shared<cbdc::buffer>(
                                    std::move(resp));
                                net->send(resp_ptr, peer_id);
                            });
                    } else {
                        ret = Server::blocking_call(std::move(*msg.m_pkt));
                    }

                    return ret;
                });

            if(!handler_thread.has_value()) {
                return false;
            }

            m_handler_thread = std::move(handler_thread.value());

            return true;
        }

      private:
        std::shared_ptr<network::connection_manager> m_net;
        network::endpoint_t m_listen_endpoint;
        std::thread m_handler_thread;
    };

    /// TCP RPC server which implements blocking request handling logic.
    template<typename Request, typename Response>
    using blocking_tcp_server = tcp_server<blocking_server<Request, Response>>;

    /// TCP RPC server which implements asynchronous request handling logic.
    template<typename Request, typename Response>
    using async_tcp_server = tcp_server<async_server<Request, Response>>;
}

#endif
