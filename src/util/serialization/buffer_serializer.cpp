// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "buffer_serializer.hpp"

#include <cstring>

namespace cbdc {
    buffer_serializer::buffer_serializer(cbdc::buffer& pkt) : m_pkt(pkt) {}

    buffer_serializer::operator bool() const {
        return m_valid;
    }

    void buffer_serializer::advance_cursor(size_t len) {
        m_cursor += len;
    }

    void buffer_serializer::reset() {
        m_cursor = 0;
        m_valid = true;
    }

    [[nodiscard]] auto buffer_serializer::end_of_buffer() const -> bool {
        return m_cursor >= m_pkt.size();
    }

    auto buffer_serializer::write(const void* data, size_t len) -> bool {
        if(m_cursor + len > m_pkt.size()) {
            m_pkt.extend(m_cursor + len - m_pkt.size());
        }
        std::memcpy(m_pkt.data_at(m_cursor), data, len);
        m_cursor += len;
        return true;
    }

    auto buffer_serializer::read(void* data, size_t len) -> bool {
        if(m_cursor + len > m_pkt.size()) {
            m_valid = false;
            return false;
        }
        std::memcpy(data, m_pkt.data_at(m_cursor), len);
        m_cursor += len;
        return true;
    }
}
