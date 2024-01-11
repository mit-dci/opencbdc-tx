// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_AGENT_RUNNERS_PY_SERVER_H_
#define OPENCBDC_TX_SRC_PARSEC_AGENT_RUNNERS_PY_SERVER_H_

#include "parsec/agent/server_interface.hpp"
#include "util/rpc/tcp_server.hpp"

namespace cbdc::parsec::agent::rpc {
    /// RPC py_server for an agent running a Lua exector. Manages retrying
    /// function execution if it fails due to a transient error.
    class py_server : public server_interface {
      public:
        /// Underlying RPC py_server type alias for this implementation.
        using server_type = cbdc::rpc::async_tcp_server<request, response>;

        /// Constructor. Registers the agent implementation with the
        /// RPC py_server using a request handler callback.
        /// \param srv pointer to an asynchronous RPC py_server.
        /// \param broker broker instance.
        /// \param log log instance.
        /// \param cfg system configuration options.
        py_server(std::unique_ptr<server_type> srv,
                  std::shared_ptr<broker::interface> broker,
                  std::shared_ptr<logging::log> log,
                  const cbdc::parsec::config& cfg);

        /// Stops the py_server.
        ~py_server() override;

        /// Initializes the py_server. Starts listening for and processing
        /// requests.
        /// \return true if the py_server was started successfully.
        auto init() -> bool override;

        py_server(const py_server&) = delete;
        auto operator=(const py_server&) -> py_server& = delete;
        py_server(py_server&&) = delete;
        auto operator=(py_server&&) -> py_server& = delete;

      private:
        std::unique_ptr<server_type> m_srv;

        auto request_handler(request req,
                             server_type::response_callback_type callback)
            -> bool;
    };
}

#endif
