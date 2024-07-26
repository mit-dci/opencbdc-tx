// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COORDINATOR_CLIENT_H_
#define OPENCBDC_TX_SRC_COORDINATOR_CLIENT_H_

#include "interface.hpp"
#include "messages.hpp"
#include "util/rpc/tcp_client.hpp"

namespace cbdc::coordinator::rpc {
    /// RPC client for a coordinator.
    class client : public interface {
      public:
        /// Constructor.
        /// \param endpoints RPC server endpoints for the coordinator cluster.
        explicit client(std::vector<network::endpoint_t> endpoints);

        client() = delete;
        ~client() override = default;
        client(const client&) = delete;
        auto operator=(const client&) -> client& = delete;
        client(client&&) = delete;
        auto operator=(client&&) -> client& = delete;

        /// Initializes the RPC client by connecting to the coordinator cluster
        /// and starting a response handler thread.
        /// \return false if there is only one coordinator endpoint and
        ///         connecting to it failed. Otherwise true.
        auto init() -> bool;

        /// Requests execution of the given transaction using the coordinator
        /// cluster. \see interface::execute_transaction.
        /// \param tx transaction to execution.
        /// \param result_callback function to call when execution result is
        ///                        available.
        /// \return true if the RPC request was sent to the cluster
        ///         successfully.
        auto
        execute_transaction(transaction::compact_tx tx,
                            callback_type result_callback) -> bool override;

      private:
        std::unique_ptr<cbdc::rpc::tcp_client<request, response>> m_client;
    };
}

#endif
