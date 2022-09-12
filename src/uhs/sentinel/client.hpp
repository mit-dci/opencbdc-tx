// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SENTINEL_CLIENT_H_
#define OPENCBDC_TX_SRC_SENTINEL_CLIENT_H_

#include "async_interface.hpp"
#include "uhs/transaction/transaction.hpp"
#include "util/common/config.hpp"
#include "util/network/connection_manager.hpp"
#include "util/rpc/tcp_client.hpp"

namespace cbdc::sentinel::rpc {
    /// \brief TCP RPC client for sentinels.
    class client : public interface, public async_interface {
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
        /// \param error_fatal treat connection errors as fatal. See
        ///                    tcp_client::init for further explanation.
        /// \return true if initialization succeeded.
        /// \see \ref cbdc::rpc::tcp_client::init(std::optional<bool>)
        auto init(std::optional<bool> error_fatal = std::nullopt) -> bool;

        /// Result type from execute_transaction.
        using execute_result_type
            = std::optional<cbdc::sentinel::execute_response>;

        /// Send a transaction to the sentinel and return the response.
        /// \param tx transaction to send to the sentinel.
        /// \return the response from the sentinel.
        auto execute_transaction(transaction::full_tx tx)
            -> execute_result_type override;

        /// Send a transaction to the sentinel and return the response via a
        /// callback function asynchronously.
        /// \param tx transaction to send to the sentinel.
        /// \param result_callback callback function to call with the result.
        /// \return true if the request was sent successfully.
        auto execute_transaction(
            transaction::full_tx tx,
            std::function<void(execute_result_type)> result_callback)
            -> bool override;

        /// Return type from transaction validation.
        using validate_result_type = std::optional<validate_response>;

        /// Send a transaction to the sentinel for validation and return the
        /// response.
        /// \param tx transaction to validate and attest to.
        /// \return sentinel attestation on the given transaction or
        ///         std::nullopt if the transaction was invalid.
        auto validate_transaction(transaction::full_tx tx)
            -> validate_result_type override;

        /// Send a transaction to the sentinel for validation and return the
        /// response via a callback function asynchronously.
        /// \param tx transaction to validate and attest to.
        /// \param result_callback callback function to call with the result.
        /// \return true if the request was sent successfully.
        auto validate_transaction(
            transaction::full_tx tx,
            std::function<void(validate_result_type)> result_callback)
            -> bool override;

      private:
        cbdc::config::options m_opts;
        std::shared_ptr<logging::log> m_logger;

        cbdc::rpc::tcp_client<request, response> m_client;
    };
}
#endif // OPENCBDC_TX_SRC_SENTINEL_CLIENT_H_
