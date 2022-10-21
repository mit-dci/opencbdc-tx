// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_EVENT_HANDLER_H_
#define OPENCBDC_TX_SRC_RPC_EVENT_HANDLER_H_

#include <cstdint>
#include <optional>
#include <vector>

namespace cbdc::rpc {
    /// Event handler interface for tracking events on non-blocking file
    /// descriptors.
    class event_handler {
      public:
        /// Type of event to register interest in.
        enum class event_type {
            /// Remove file descriptor.
            remove,
            /// Ready to read event.
            in,
            /// Ready to write event.
            out,
            /// Read and write events.
            inout,
        };

        virtual ~event_handler() = default;

        /// Type alias for an event. First value is the file descriptor, second
        /// is true if the event is a timeout.
        using event = std::pair<int, bool>;

        /// Initializes the event handler.
        /// \return true if initialization succeeded.
        virtual auto init() -> bool = 0;

        /// Sets the timeout for poll to return even if there are no events.
        /// \param timeout_ms timeout in milliseconds. 0 to disable timeout.
        virtual void set_timeout(long timeout_ms) = 0;

        /// Registers a file descriptor to track for events.
        /// \param fd file descriptor.
        /// \param et event type.
        virtual void register_fd(int fd, event_type et) = 0;

        /// Wait for events on tracked file descriptors. Blocks until at least
        /// one event is available, or the timeout expires.
        /// \return list of events, or std::nullopt if there was an error
        ///         during polling.
        virtual auto poll() -> std::optional<std::vector<event>> = 0;
    };
}

#endif
