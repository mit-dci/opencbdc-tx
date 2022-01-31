// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SENTINEL_CLIENT_H_
#define OPENCBDC_TX_SRC_SENTINEL_CLIENT_H_

#include "interface.hpp"
#include "uhs/transaction/transaction.hpp"
#include "util/common/config.hpp"
#include "util/network/connection_manager.hpp"
#include "util/rpc/tcp_client.hpp"

namespace cbdc::sentinel::rpc {
    /// \brief TCP RPC client for sentinels.
    class client : public interface {
      public:
        /// Constructor.
        /// \param endpoints sentinel cluster RPC endpoints.
        /// \param logger pointer shared logger.
        client(std::vector<network::endpoint_t> endpoints,
               std::shared_ptr<logging::log> logger);

        ~client() override = default;

        client() = delete;
        client(const client&) = delete;
        auto operator=(const client&) -> client& = delete;
        client(client&&) = delete;
        auto operator=(client&&) -> client& = delete;

        /// Initializes the client. Establishes a connection to the sentinel.
        /// \return true if initialization succeeded.
        auto init() -> bool;

        /// Result type from execute_transaction.
        using result_type = std::optional<cbdc::sentinel::response>;

        /// Send a transaction to the sentinel and return the response.
        /// \param tx transaction to send to the sentinel.
        /// \return the response from the sentinel.
        auto execute_transaction(transaction::full_tx tx)
            -> result_type override;

        /// Send a transaction to the sentinel and return the response via a
        /// callback function asynchronously.
        /// \param tx transaction to send to the sentinel.
        /// \param result_callback callback function to call with the result.
        /// \return true if the request was sent successfully.
        auto
        execute_transaction(transaction::full_tx tx,
                            std::function<void(result_type)> result_callback)
            -> bool;

      private:
        cbdc::config::options m_opts;
        std::shared_ptr<logging::log> m_logger;

        cbdc::rpc::tcp_client<request, response> m_client;
    };
}
#endif // OPENCBDC_TX_SRC_SENTINEL_CLIENT_H_
