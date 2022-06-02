// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SENTINEL_CONTROLLER_H_
#define OPENCBDC_TX_SRC_SENTINEL_CONTROLLER_H_

#include "server.hpp"
#include "uhs/sentinel/async_interface.hpp"
#include "uhs/sentinel/client.hpp"
#include "uhs/sentinel/interface.hpp"
#include "uhs/transaction/transaction.hpp"
#include "util/common/config.hpp"
#include "util/network/connection_manager.hpp"

#include <memory>
#include <random>

#include <secp256k1.h>
#include <secp256k1_bulletproofs.h>

namespace cbdc::sentinel {
    /// Sentinel implementation.
    class controller : public interface {
      public:
        controller() = delete;
        controller(const controller&) = delete;
        auto operator=(const controller&) -> controller& = delete;
        controller(controller&&) = delete;
        auto operator=(controller&&) -> controller& = delete;

        /// Constructor.
        /// \param sentinel_id the running ID of this shard.
        /// \param opts pointer to configuration options.
        /// \param logger pointer shared logger.
        controller(uint32_t sentinel_id,
                   config::options opts,
                   std::shared_ptr<logging::log> logger);

        ~controller() override = default;

        /// Initializes the controller. Establishes connections to the shards
        /// \return true if initialization succeeded.
        auto init() -> bool;

        /// Validate transaction, forward it to shards for processing,
        /// and return the validation result to send back to the originating
        /// client.
        /// \param tx transaction to execute.
        /// \return response with the transaction status to send to the client.
        auto execute_transaction(transaction::full_tx tx)
            -> std::optional<cbdc::sentinel::execute_response> override;

        /// Validate transaction and generate a sentinel attestation if the
        /// transaction is valid.
        /// \param tx transaction to validate and attest to.
        /// \return sentinel attestation for the given transaction, or
        ///         std::nullopt if the transaction is invalid.
        auto validate_transaction(transaction::full_tx tx)
            -> std::optional<validate_response> override;

      private:
        uint32_t m_sentinel_id;
        cbdc::config::options m_opts;
        std::shared_ptr<logging::log> m_logger;

        std::vector<shard_info> m_shard_data;

        cbdc::network::connection_manager m_shard_network;

        std::unique_ptr<rpc::server> m_rpc_server;

        std::unique_ptr<secp256k1_context,
                        decltype(&secp256k1_context_destroy)>
            m_secp{secp256k1_context_create(SECP256K1_CONTEXT_SIGN),
                   &secp256k1_context_destroy};

        std::vector<std::unique_ptr<sentinel::rpc::client>>
            m_sentinel_clients{};

        std::random_device m_r{};
        std::default_random_engine m_rand{m_r()};
        std::uniform_int_distribution<size_t> m_dist{};
        std::uniform_int_distribution<size_t> m_shard_dist{};
        std::mutex m_rand_mut;

        privkey_t m_privkey{};

        void validate_result_handler(async_interface::validate_result v_res,
                                     const transaction::full_tx& tx,
                                     transaction::compact_tx ctx,
                                     std::unordered_set<size_t> requested);

        void gather_attestations(const transaction::full_tx& tx,
                                 const transaction::compact_tx& ctx,
                                 std::unordered_set<size_t> requested);

        void send_compact_tx(const transaction::compact_tx& ctx);

        void send_transaction(const transaction::full_tx& tx);
    };
}

#endif // OPENCBDC_TX_SRC_SENTINEL_CONTROLLER_H_
