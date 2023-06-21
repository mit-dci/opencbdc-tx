// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_TICKET_MACHINE_CLIENT_H_
#define OPENCBDC_TX_SRC_PARSEC_TICKET_MACHINE_CLIENT_H_

#include "interface.hpp"
#include "messages.hpp"
#include "util/rpc/tcp_client.hpp"

namespace cbdc::parsec::ticket_machine::rpc {
    /// RPC client for a remote ticket machine.
    class client : public interface {
      public:
        /// Constructor.
        /// \param endpoints RPC server endpoints for the ticket machine
        ///                  cluster.
        explicit client(std::vector<network::endpoint_t> endpoints);

        client() = delete;
        ~client() override = default;
        client(const client&) = delete;
        auto operator=(const client&) -> client& = delete;
        client(client&&) = delete;
        auto operator=(client&&) -> client& = delete;

        /// Initializes the underlying TCP client.
        /// \return true if the client initialized successfully.
        auto init() -> bool;

        /// Requests a new batch of ticket numbers from the remote ticket
        /// machine. Always returns a single ticket number (range size of 1).
        /// Caches ticket numbers to avoid making an RPC request per call. If a
        /// ticket number is available in the cache, calls the callback before
        /// returning.
        /// \param result_callback function to call with the new ticket number.
        /// \return true if the request was initiated successfully.
        auto get_ticket_number(get_ticket_number_callback_type result_callback)
            -> bool override;

      private:
        std::unique_ptr<cbdc::rpc::tcp_client<request, response>> m_client;
        std::queue<ticket_number_type> m_tickets;
        bool m_fetching_tickets{false};

        mutable std::mutex m_mut;

        std::queue<get_ticket_number_callback_type> m_callbacks;

        auto fetch_tickets() -> bool;

        void handle_ticket_numbers(ticket_number_range_type range);
    };
}

#endif
