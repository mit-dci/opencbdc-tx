// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_EPOLL_EVENT_HANDLER_H_
#define OPENCBDC_TX_SRC_RPC_EPOLL_EVENT_HANDLER_H_

#include "event_handler.hpp"

#include <set>

namespace cbdc::rpc {
    /// Event handler implementation using Linux epoll.
    class epoll_event_handler : public event_handler {
      public:
        epoll_event_handler() = default;
        ~epoll_event_handler() override;

        epoll_event_handler(const epoll_event_handler&) = default;
        auto operator=(const epoll_event_handler&)
            -> epoll_event_handler& = default;
        epoll_event_handler(epoll_event_handler&&) = default;
        auto
        operator=(epoll_event_handler&&) -> epoll_event_handler& = default;

        /// \copydoc event_handler::init
        auto init() -> bool override;

        /// \copydoc event_handler::set_timeout
        void set_timeout(long timeout_ms) override;

        /// \copydoc event_handler::register_fd
        void register_fd(int fd, event_type et) override;

        /// \copydoc event_handler::poll
        auto poll() -> std::optional<std::vector<event>> override;

      private:
        int m_epoll{};
        long m_timeout_ms{1000};
        bool m_timeout_enabled{true};
        std::set<int> m_tracked;
    };
}

#endif
