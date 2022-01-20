// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SENTINEL_CONTROLLER_H_
#define OPENCBDC_TX_SRC_SENTINEL_CONTROLLER_H_

#include "common/config.hpp"
#include "interface.hpp"
#include "network/connection_manager.hpp"
#include "server.hpp"

#include <memory>

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
            -> std::optional<cbdc::sentinel::response> override;

      private:
        uint32_t m_sentinel_id;
        cbdc::config::options m_opts;
        std::shared_ptr<logging::log> m_logger;

        std::vector<shard_info> m_shard_data;

        cbdc::network::connection_manager m_shard_network;

        std::unique_ptr<rpc::server> m_rpc_server;

        void send_transaction(const transaction::full_tx& tx);
    };
}

#endif // OPENCBDC_TX_SRC_SENTINEL_CONTROLLER_H_
