// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_CLIENT_ATOMIZER_CLIENT_H_
#define OPENCBDC_TX_SRC_CLIENT_ATOMIZER_CLIENT_H_

#include "client.hpp"
#include "uhs/atomizer/watchtower/client.hpp"

namespace cbdc {
    /// Client for interacting with the atomizer architecture.
    class atomizer_client : public client {
      public:
        /// Constructor.
        /// \param opts configuration options.
        /// \param logger pointer shared logger.
        /// \param wallet_file name of .dat file in which to store the wallet
        ///                    data.
        /// \param client_file name of .dat file in which to store the
        ///                    internal state data.
        atomizer_client(const cbdc::config::options& opts,
                        const std::shared_ptr<logging::log>& logger,
                        const std::string& wallet_file,
                        const std::string& client_file);

        ~atomizer_client() override;

        atomizer_client() = delete;
        atomizer_client(const atomizer_client&) = delete;
        auto operator=(const atomizer_client&) -> atomizer_client& = delete;
        atomizer_client(atomizer_client&&) = delete;
        auto operator=(atomizer_client&&) -> atomizer_client& = delete;

        /// \brief Update the client with the latest state from the watchtower.
        ///
        /// Queries the watchtower's client endpoint to determine whether any
        /// pending transactions or inputs have confirmed or been rejected by
        /// the system.
        /// \return false if any pending transactions have failed according
        ///         to the watchtower.
        auto sync() -> bool override;

      protected:
        /// \brief Initializes the atomizer client.
        ///
        /// Connects to the network of atomizers to allow this client to
        /// directly submit mint transactions. Initializes the watchtower
        /// client to allow this client to confirm pending transactions and
        /// imported inputs.
        /// \return true if the initialization succeeded.
        auto init_derived() -> bool override;

        /// Sends the given transaction directly to the atomizer cluster.
        /// \param mint_tx transaction to send.
        /// \return true if sending the transaction was successful.
        auto send_mint_tx(const transaction::full_tx& mint_tx)
            -> bool override;

      private:
        cbdc::network::connection_manager m_atomizer_network;
        cbdc::watchtower::blocking_client m_wc;
        std::shared_ptr<logging::log> m_logger;
        cbdc::config::options m_opts;

        using secp256k1_context_destroy_type = void (*)(secp256k1_context*);

        std::unique_ptr<secp256k1_context,
                        secp256k1_context_destroy_type>
            m_secp{secp256k1_context_create(SECP256K1_CONTEXT_NONE),
                   &secp256k1_context_destroy};
    };
}

#endif
