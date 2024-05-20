// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SENTINEL_2PC_CONTROLLER_H_
#define OPENCBDC_TX_SRC_SENTINEL_2PC_CONTROLLER_H_

#include "crypto/sha256.h"
#include "server.hpp"
#include "uhs/sentinel/async_interface.hpp"
#include "uhs/sentinel/client.hpp"
#include "uhs/sentinel/format.hpp"
#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/transaction.hpp"
#include "uhs/twophase/coordinator/client.hpp"
#include "util/common/config.hpp"
#include "util/common/hashmap.hpp"
#include "util/network/connection_manager.hpp"

#include <random>
#include <secp256k1.h>
#include <secp256k1_bppp.h>

namespace cbdc::sentinel_2pc {
    /// Manages a sentinel server for the two-phase commit architecture.
    class controller : public cbdc::sentinel::async_interface {
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
                   const config::options& opts,
                   std::shared_ptr<logging::log> logger);

        ~controller() override = default;

        /// Initializes the controller. Connects to the shard coordinator
        /// network and launches a server thread for external clients.
        /// \return true if initialization succeeded.
        auto init() -> bool;

        /// Statically validates a transaction, submits it the shard
        /// coordinator network, and returns the result via a callback
        /// function.
        /// \param tx transaction to submit.
        /// \param result_callback function to call with the execution result.
        /// \return false if the sentinel was unable to forward the transaction
        ///         to a coordinator.
        auto execute_transaction(transaction::full_tx tx,
                                 execute_result_callback_type result_callback)
            -> bool override;

        /// Statically validates a transaction and generates a sentinel
        /// attestation if the transaction is valid.
        /// \param tx transaction to validate.
        /// \param result_callback function to call with the attestation or
        ///                        std::nullopt if the transaction was invalid.
        /// \return true.
        auto
        validate_transaction(transaction::full_tx tx,
                             validate_result_callback_type result_callback)
            -> bool override;

      private:
        static void result_handler(std::optional<bool> res,
                                   const execute_result_callback_type& res_cb);

        void
        validate_result_handler(validate_result v_res,
                                const transaction::full_tx& tx,
                                execute_result_callback_type result_callback,
                                transaction::compact_tx ctx,
                                std::unordered_set<size_t> requested);

        void gather_attestations(const transaction::full_tx& tx,
                                 execute_result_callback_type result_callback,
                                 const transaction::compact_tx& ctx,
                                 std::unordered_set<size_t> requested);

        void send_compact_tx(const transaction::compact_tx& ctx,
                             execute_result_callback_type result_callback);

        uint32_t m_sentinel_id;
        cbdc::config::options m_opts;
        std::shared_ptr<logging::log> m_logger;

        std::unique_ptr<cbdc::sentinel::rpc::async_server> m_rpc_server;

        using secp256k1_context_destroy_type = void (*)(secp256k1_context*);

        std::unique_ptr<secp256k1_context,
                        secp256k1_context_destroy_type>
            m_secp{secp256k1_context_create(SECP256K1_CONTEXT_NONE),
                   &secp256k1_context_destroy};

        struct GensDeleter {
            explicit GensDeleter(secp256k1_context* ctx) : m_ctx(ctx) {}

            void operator()(secp256k1_bppp_generators* gens) const {
                secp256k1_bppp_generators_destroy(m_ctx, gens);
            }

            secp256k1_context* m_ctx;
        };

        /// should be set to exactly `floor(log_base(value)) + 1`.
        ///
        /// We use n_bits = 64, base = 16, so this should always be 24.
        static const inline auto generator_count = 16 + 8;

        std::unique_ptr<secp256k1_bppp_generators, GensDeleter>
            m_generators{
                secp256k1_bppp_generators_create(m_secp.get(),
                                                 generator_count),
                GensDeleter(m_secp.get())};

        std::optional<cbdc::commitment_t> m_seed_commitment {};
        cbdc::rangeproof_t m_seed_rangeproof {};

        coordinator::rpc::client m_coordinator_client;

        std::vector<std::unique_ptr<sentinel::rpc::client>>
            m_sentinel_clients{};

        std::random_device m_r{};
        std::default_random_engine m_rand{m_r()};
        std::uniform_int_distribution<size_t> m_dist{};

        static const inline auto m_random_source
            = std::make_unique<random_source>(config::random_source);

        privkey_t m_privkey{};
    };
}

#endif // OPENCBDC_TX_SRC_SENTINEL_2PC_CONTROLLER_H_
