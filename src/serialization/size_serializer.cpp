// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "size_serializer.hpp"

namespace cbdc {
    size_serializer::operator bool() const {
        return true;
    }

    void size_serializer::advance_cursor(size_t len) {
        m_cursor += len;
    }

    void size_serializer::reset() {
        m_cursor = 0;
    }

    [[nodiscard]] auto size_serializer::end_of_buffer() const -> bool {
        return false;
    }

    auto size_serializer::write(const void* /* data */, size_t len) -> bool {
        m_cursor += len;
        return true;
    }

    auto size_serializer::read(void* /* data */, size_t /* len */) -> bool {
        return false;
    }

    [[nodiscard]] auto size_serializer::size() const -> size_t {
        return m_cursor;
    }
}
