// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_AGENT_CLIENT_H_
#define OPENCBDC_TX_SRC_PARSEC_AGENT_CLIENT_H_

#include "interface.hpp"
#include "messages.hpp"
#include "util/rpc/tcp_client.hpp"

namespace cbdc::parsec::agent::rpc {
    /// RPC client for an agent.
    class client {
      public:
        /// Constructor.
        /// \param endpoints RPC server endpoints for the agent cluster.
        explicit client(std::vector<network::endpoint_t> endpoints);

        client() = delete;
        ~client() = default;
        client(const client&) = delete;
        auto operator=(const client&) -> client& = delete;
        client(client&&) = delete;
        auto operator=(client&&) -> client& = delete;

        /// Intializes the underlying TCP client.
        /// \return true if the TCP client initialized successfully.
        auto init() -> bool;

        /// Requests execution of the function at a key with the given
        /// parameters.
        /// \param function key where function bytecode is located.
        /// \param param parameter to call function with.
        /// \param is_readonly_run true if agent should skip writing any state changes.
        /// \param result_callback function to call with execution result.
        /// \return true if the request was sent successfully.
        auto exec(runtime_locking_shard::key_type function,
                  parameter_type param,
                  bool is_readonly_run,
                  const interface::exec_callback_type& result_callback)
            -> bool;

      private:
        std::unique_ptr<cbdc::rpc::tcp_client<request, response>> m_client;
    };
}

#endif
