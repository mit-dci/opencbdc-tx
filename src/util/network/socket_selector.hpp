// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_NETWORK_SOCKET_SELECTOR_H_
#define OPENCBDC_TX_SRC_NETWORK_SOCKET_SELECTOR_H_

#include "socket.hpp"

#include <array>

namespace cbdc::network {
    /// \brief Waits on a group of blocking sockets to be ready for read
    ///        operations.
    ///
    /// Utility class for waiting on multiple blocking sockets. Users add
    /// sockets to the selector and block on a \ref wait call. The call
    /// unblocks when any of the sockets in the selector are ready to receive
    /// data.
    class socket_selector {
      public:
        /// Constructs an empty socket selector.
        socket_selector() = default;
        ~socket_selector();
        socket_selector(const socket_selector&) = delete;
        auto operator=(const socket_selector&) -> socket_selector& = delete;
        socket_selector(socket_selector&&) = delete;
        auto operator=(socket_selector&&) -> socket_selector& = delete;

        /// Sets-up the socket selector. Must be called before the selector is
        /// used.
        /// \return true if the setup was successful.
        auto init() -> bool;

        /// Adds a socket to the selector so that it is checked for
        /// events after a call to wait.
        /// \param sock the socket to add to the selector
        /// \return true if there was space in the selector to add the socket
        auto add(const socket& sock) -> bool;

        /// Blocks until at least one socket in the selector is ready
        /// to perform a read operation.
        /// \return true if there is a ready socket
        auto wait() -> bool;

        /// Unblocks a blocked wait() call.
        void unblock();

      private:
        fd_set m_fds{};
        fd_set m_ready_fds{};
        int m_fd_max{-1};
        std::array<int, 2> m_unblock_fds{-1, -1};

        auto add(int fd) -> bool;
    };
}

#endif
