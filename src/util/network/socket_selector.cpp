// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "socket_selector.hpp"

#include <cassert>
#include <unistd.h>

namespace cbdc::network {
    auto socket_selector::add(const socket& sock) -> bool {
        return add(sock.m_sock_fd);
    }

    auto socket_selector::wait() -> bool {
        m_ready_fds = m_fds;
        const auto nfds
            = select(m_fd_max + 1, &m_ready_fds, nullptr, nullptr, nullptr);
        // This macro does not bounds-check fd within m_fds. Since it comes
        // from the system library there's nothing we can do about it.
        // m_unblock_fds is set in init() and checked for validity so this
        // should still be safe.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        auto unblock = FD_ISSET(m_unblock_fds[0], &m_ready_fds);
        if(static_cast<int>(unblock) != 0) {
            auto dummy = char();
            [[maybe_unused]] auto res
                = read(m_unblock_fds[0], &dummy, sizeof(dummy));
            assert(res != -1);
            return false;
        }
        return nfds > 0;
    }

    auto socket_selector::add(int fd) -> bool {
        if(fd >= FD_SETSIZE) {
            return false;
        }
        // This macro does not bounds-check fd within m_fds. Since it comes
        // from the system library there's nothing we can do about it.
        // We check the bounds directly above, so this should be safe.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        FD_SET(fd, &m_fds);
        m_fd_max = std::max(m_fd_max, fd);
        return true;
    }

    auto socket_selector::init() -> bool {
        auto pipe_res = pipe(m_unblock_fds.data());
        if(pipe_res != 0) {
            return false;
        }
        auto add_res = add(m_unblock_fds[0]);
        return add_res;
    }

    void socket_selector::unblock() {
        if(m_unblock_fds[1] != -1) {
            static constexpr auto dummy_byte = char();
            [[maybe_unused]] auto res
                = write(m_unblock_fds[1], &dummy_byte, sizeof(dummy_byte));
            assert(res != -1);
        }
    }

    socket_selector::~socket_selector() {
        unblock();
        close(m_unblock_fds[0]);
        close(m_unblock_fds[1]);
    }
}
