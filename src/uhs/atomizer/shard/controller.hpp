// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SHARD_CONTROLLER_H_
#define OPENCBDC_TX_SRC_SHARD_CONTROLLER_H_

#include "shard.hpp"
#include "uhs/atomizer/archiver/client.hpp"
#include "uhs/atomizer/atomizer/block.hpp"
#include "util/common/config.hpp"
#include "util/network/connection_manager.hpp"

#include <memory>
#include <secp256k1.h>

namespace cbdc::shard {
    /// Wrapper for the shard executable implementation.
    class controller {
      public:
        controller() = delete;
        controller(const controller&) = delete;
        auto operator=(const controller&) -> controller& = delete;
        controller(controller&&) = delete;
        auto operator=(controller&&) -> controller& = delete;

        /// Constructor.
        /// \param shard_id the running ID of this shard.
        /// \param opts pointer to configuration options.
        /// \param logger pointer shared logger.
        controller(uint32_t shard_id,
                   config::options opts,
                   std::shared_ptr<logging::log> logger);

        ~controller();

        /// Initializes the controller. Opens client connections to archiver,
        /// watchtower, and atomizer. Establishes a server for the controllers
        /// shard. Configures network handlers. If initialization fails,
        /// returns false and logs errors.
        /// \return true if initialization succeeded.
        auto init() -> bool;

      private:
        uint32_t m_shard_id;
        cbdc::config::options m_opts;
        std::shared_ptr<logging::log> m_logger;
        shard m_shard;

        cbdc::network::connection_manager m_watchtower_network;
        cbdc::network::connection_manager m_atomizer_network;
        cbdc::network::connection_manager m_shard_network;

        std::thread m_shard_server;
        std::thread m_atomizer_client;

        cbdc::archiver::client m_archiver_client;

        blocking_queue<network::message_t> m_request_queue;
        std::vector<std::thread> m_handler_threads;

        std::ofstream m_audit_log;
        std::thread m_audit_thread;

        auto server_handler(cbdc::network::message_t&& pkt)
            -> std::optional<cbdc::buffer>;
        auto atomizer_handler(cbdc::network::message_t&& pkt)
            -> std::optional<cbdc::buffer>;
        void request_consumer();
        void audit();
    };
}

#endif // OPENCBDC_TX_SRC_SHARD_CONTROLLER_H_
