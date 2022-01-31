// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/** \file client.hpp
 * Client helpers for interfacing with a watchtower server.
 */

#ifndef OPENCBDC_TX_SRC_WATCHTOWER_CLIENT_H_
#define OPENCBDC_TX_SRC_WATCHTOWER_CLIENT_H_

#include "uhs/atomizer/watchtower/messages.hpp"
#include "uhs/atomizer/watchtower/status_update_messages.hpp"
#include "util/common/blocking_queue.hpp"
#include "util/common/config.hpp"
#include "util/network/connection_manager.hpp"
#include "util/serialization/format.hpp"

namespace cbdc::watchtower {
    /// Client to synchronously request information from the watchtower.
    class blocking_client {
      public:
        blocking_client() = delete;
        /// Constructor.
        /// \param ep Watchtower endpoint.
        explicit blocking_client(network::endpoint_t ep);

        /// Attempts to connect to the watchtower.
        /// \return true if the connection was successful.
        auto init() -> bool;

        ~blocking_client();

        blocking_client(const blocking_client&) = delete;
        auto operator=(const blocking_client&) -> blocking_client& = delete;
        blocking_client(blocking_client&&) = delete;
        auto operator=(blocking_client&&) -> blocking_client& = delete;

        /// Sends a request_best_block_height to the Watchtower. Blocks until
        /// the Watchtower sends a response.
        /// \return the response from the Watchtower.
        auto request_best_block_height()
            -> std::shared_ptr<best_block_height_response>;

        /// Sends a StatusUpdateRequest to the Watchtower. Blocks until the
        /// Watchtower sends a response.
        /// \param req request to send to the Watchtower.
        /// \return the response from the Watchtower.
        auto request_status_update(const status_update_request& req)
            -> std::shared_ptr<status_request_check_success>;

      private:
        network::endpoint_t m_ep;
        cbdc::network::connection_manager m_network;
        std::thread m_client_thread;
        blocking_queue<std::shared_ptr<response>> m_res_q;
    };

    /// Client to asynchronously request information from the watchtower.
    class async_client {
      public:
        async_client() = delete;
        /// Constructor.
        /// \param ep Watchtower endpoint.
        explicit async_client(network::endpoint_t ep);

        ~async_client();

        async_client(const async_client&) = delete;
        auto operator=(const async_client&) -> async_client& = delete;
        async_client(async_client&&) = delete;
        auto operator=(async_client&&) -> async_client& = delete;

        /// Attempts to connect to the watchtower.
        /// \return true if the connection was successful.
        auto init() -> bool;

        /// Sends a request_best_block_height to the Watchtower.
        void request_best_block_height();

        /// Sends a StatusUpdateRequest to the Watchtower.
        void request_status_update(const status_update_request& req);

        using status_update_response_handler_t = std::function<void(
            std::shared_ptr<status_request_check_success>&&)>;

        /// Sets or replaces the handler for asynchronously delivered
        /// StatusUpdateResponses.
        /// \param handler the handler function that the client should call whenever it receives a new StatusUpdateResponse.
        void set_status_update_handler(
            const status_update_response_handler_t& handler);

        using best_block_height_handler_t = std::function<void(
            std::shared_ptr<best_block_height_response>&&)>;

        /// Sets or replaces the handler for asynchronously delivered
        /// StatusUpdateResponses.
        /// \param handler the handler function that the client should call whenever it receives a new StatusUpdateResponse.
        void
        set_block_height_handler(const best_block_height_handler_t& handler);

      private:
        network::endpoint_t m_ep;
        cbdc::network::connection_manager m_network;
        std::thread m_client_thread;
        std::thread m_handler_thread;
        bool m_handler_running{false};
        blocking_queue<std::shared_ptr<response>> m_res_q;
        status_update_response_handler_t m_su_handler;
        best_block_height_handler_t m_bbh_handler;
    };
}

#endif // OPENCBDC_TX_SRC_WATCHTOWER_CLIENT_H_
