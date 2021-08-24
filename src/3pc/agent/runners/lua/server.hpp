// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CBDC_UNIVERSE0_SRC_3PC_AGENT_RUNNERS_LUA_SERVER_H_
#define CBDC_UNIVERSE0_SRC_3PC_AGENT_RUNNERS_LUA_SERVER_H_

#include "agent/server_interface.hpp"
#include "util/rpc/tcp_server.hpp"

namespace cbdc::threepc::agent::rpc {
    /// RPC server for an agent running a Lua exector. Manages retrying
    /// function execution if it fails due to a transient error.
    class server : public server_interface {
      public:
        /// Underlying RPC server type alias for this implementation.
        using server_type = cbdc::rpc::async_tcp_server<request, response>;

        /// Constructor. Registers the agent implementation with the
        /// RPC server using a request handler callback.
        /// \param srv pointer to an asynchronous RPC server.
        /// \param broker broker instance.
        /// \param log log instance.
        /// \param tel telemetry instance.
        /// \param cfg system configuration options.
        server(std::unique_ptr<server_type> srv,
               std::shared_ptr<broker::interface> broker,
               std::shared_ptr<logging::log> log,
               std::shared_ptr<cbdc::telemetry> tel,
               const cbdc::threepc::config& cfg);

        /// Stops the server.
        ~server() override;

        /// Initializes the server. Starts listening for and processing
        /// requests.
        /// \return true if the server was started successfully.
        auto init() -> bool override;

        server(const server&) = delete;
        auto operator=(const server&) -> server& = delete;
        server(server&&) = delete;
        auto operator=(server&&) -> server& = delete;

      private:
        std::unique_ptr<server_type> m_srv;

        auto request_handler(request req,
                             server_type::response_callback_type callback)
            -> bool;
    };
}

#endif
