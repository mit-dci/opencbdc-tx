// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_WATCHTOWER_CONTROLLER_H_
#define OPENCBDC_TX_SRC_WATCHTOWER_CONTROLLER_H_

#include "archiver/client.hpp"
#include "common/config.hpp"
#include "network/connection_manager.hpp"
#include "serialization/format.hpp"
#include "watchtower.hpp"

namespace cbdc::watchtower {
    /// Wrapper for the watchtower executable implementation.
    class controller {
      public:
        controller() = delete;
        controller(const controller&) = delete;
        auto operator=(const controller&) -> controller& = delete;
        controller(controller&&) = delete;
        auto operator=(controller&&) -> controller& = delete;

        /// Constructor.
        /// \param watchtower_id the running ID of this watchtower.
        /// \param opts configuration options.
        /// \param log pointer to shared logger.
        controller(uint32_t watchtower_id,
                   config::options opts,
                   const std::shared_ptr<logging::log>& log);

        ~controller();

        /// Initializes the controller.
        /// \return true if initialization succeeded.
        auto init() -> bool;

      private:
        uint32_t m_watchtower_id;
        cbdc::config::options m_opts;
        std::shared_ptr<logging::log> m_logger;

        watchtower m_watchtower;
        uint64_t m_last_blk_height{0};

        cbdc::network::connection_manager m_internal_network;
        cbdc::network::connection_manager m_external_network;
        cbdc::network::connection_manager m_atomizer_network;

        cbdc::archiver::client m_archiver_client;

        std::thread m_internal_server;
        std::thread m_external_server;
        std::thread m_atomizer_thread;

        void connect_atomizers();
        auto atomizer_handler(cbdc::network::message_t&& pkt)
            -> std::optional<cbdc::buffer>;
        auto internal_server_handler(cbdc::network::message_t&& pkt)
            -> std::optional<cbdc::buffer>;
        auto external_server_handler(cbdc::network::message_t&& pkt)
            -> std::optional<cbdc::buffer>;
    };
}

#endif // OPENCBDC_TX_SRC_WATCHTOWER_CONTROLLER_H_
