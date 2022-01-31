// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SENTINEL_2PC_CONTROLLER_H_
#define OPENCBDC_TX_SRC_SENTINEL_2PC_CONTROLLER_H_

#include "crypto/sha256.h"
#include "interface.hpp"
#include "server.hpp"
#include "uhs/sentinel/format.hpp"
#include "uhs/transaction/messages.hpp"
#include "uhs/twophase/coordinator/client.hpp"
#include "util/common/config.hpp"
#include "util/common/hashmap.hpp"
#include "util/network/connection_manager.hpp"

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
                                 result_callback_type result_callback)
            -> bool override;

      private:
        static void result_handler(std::optional<bool> res,
                                   const result_callback_type& res_cb);

        uint32_t m_sentinel_id;
        cbdc::config::options m_opts;
        std::shared_ptr<logging::log> m_logger;

        std::unique_ptr<cbdc::sentinel::rpc::async_server> m_rpc_server;

        coordinator::rpc::client m_coordinator_client;
    };
}

#endif // OPENCBDC_TX_SRC_SENTINEL_2PC_CONTROLLER_H_
