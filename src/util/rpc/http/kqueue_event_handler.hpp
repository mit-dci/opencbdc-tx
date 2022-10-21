// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_KQUEUE_EVENT_HANDLER_H_
#define OPENCBDC_TX_SRC_RPC_KQUEUE_EVENT_HANDLER_H_

#include "event_handler.hpp"

#include <map>
#include <set>
#include <sys/event.h>

namespace cbdc::rpc {
    /// Event handler implementation using BSD kqueue.
    class kqueue_event_handler final : public event_handler {
      public:
        kqueue_event_handler() = default;
        ~kqueue_event_handler();

        /// \copydoc event_handler::init
        auto init() -> bool override;

        /// \copydoc event_handler::set_timeout
        void set_timeout(long timeout_ms) override;

        /// \copydoc event_handler::register_fd
        void register_fd(int fd, event_type et) override;

        /// \copydoc event_handler::poll
        auto poll() -> std::optional<std::vector<event>> override;

      private:
        int m_kq{};
        long m_timeout_ms{1000};
        bool m_timeout_enabled{true};
        std::vector<struct kevent> m_evs;
    };
}

#endif
