// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_CLIENT_TWOPHASE_CLIENT_H_
#define OPENCBDC_TX_SRC_CLIENT_TWOPHASE_CLIENT_H_

#include "client.hpp"
#include "uhs/twophase/coordinator/client.hpp"
#include "uhs/twophase/locking_shard/status_client.hpp"

namespace cbdc {
    /// Client for interacting with the 2PC architecture.
    class twophase_client : public client {
      public:
        /// Constructor.
        /// \param opts configuration options.
        /// \param logger pointer to shared logger.
        /// \param wallet_file name of .dat file in which to store the wallet
        ///                    data.
        /// \param client_file name of .dat file in which to store the
        ///                    internal state data.
        twophase_client(const cbdc::config::options& opts,
                        const std::shared_ptr<logging::log>& logger,
                        const std::string& wallet_file,
                        const std::string& client_file);

        ~twophase_client() override = default;

        twophase_client() = delete;
        twophase_client(const twophase_client&) = delete;
        auto operator=(const twophase_client&) -> twophase_client& = delete;
        twophase_client(twophase_client&&) = delete;
        auto operator=(twophase_client&&) -> twophase_client& = delete;

        /// \brief Update the client with the latest state from the shard
        ///        network.
        ///
        /// Queries the shards' read-only endpoints to determine whether the
        /// transaction processing system has confirmed any of this client's
        /// pending transactions or inputs.
        /// \return true if querying the shards was successful.
        auto sync() -> bool override;

        /// \brief Checks the shard network for the status of a specific
        ///        transaction.
        ///
        /// Queries the shards' read-only endpoints to determine whether the
        /// transaction processing system has confirmed a specific transaction.
        /// \return true if the shards have confirmed the given transaction,
        ///         false if the transaction is unknown to the shards.
        auto check_tx_id(const hash_t& tx_id) -> std::optional<bool>;

        /// \brief Checks the shard network for the status of a specific UHS
        ///        ID.
        ///
        /// Queries the shards' read-only endpoints to determine whether a
        /// specific UHS ID is unspent.
        /// \return true if the shards confirm the given UHS ID is unspent.
        auto check_unspent(const hash_t& uhs_id) -> std::optional<bool>;

      protected:
        /// \brief Initializes the 2PC architecture client.
        ///
        /// Connects to the coordinator network to allow this client to
        /// directly submit minting transactions. Initializes the locking
        /// shard read-only client to allow this client to confirm transactions
        /// and imported inputs.
        /// \return true if the initialization succeeded.
        auto init_derived() -> bool override;

        /// Sends the given mint transaction directly to a coordinator cluster.
        /// \param mint_tx transaction to send.
        /// \return true if the transaction was sent successfully.
        auto send_mint_tx(const transaction::full_tx& mint_tx)
            -> bool override;

      private:
        coordinator::rpc::client m_coordinator_client;
        cbdc::locking_shard::rpc::status_client m_shard_status_client;
        std::shared_ptr<logging::log> m_logger;
        cbdc::config::options m_opts;

        static constexpr auto m_client_timeout
            = std::chrono::milliseconds(5000);
    };
}

#endif
