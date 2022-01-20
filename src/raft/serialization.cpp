// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "serialization.hpp"

#include <cstring>
#include <libnuraft/buffer.hxx>
#include <vector>

namespace cbdc {
    nuraft_serializer::nuraft_serializer(nuraft::buffer& buf) : m_buf(buf) {
        nuraft_serializer::reset();
    }

    nuraft_serializer::operator bool() const {
        return m_valid;
    }

    void nuraft_serializer::advance_cursor(size_t len) {
        m_buf.pos(m_buf.pos() + len);
    }

    void nuraft_serializer::reset() {
        m_buf.pos(0);
        m_valid = true;
    }

    [[nodiscard]] auto nuraft_serializer::end_of_buffer() const -> bool {
        return m_buf.pos() >= m_buf.size();
    }

    auto nuraft_serializer::write(const void* data, size_t len) -> bool {
        if(m_buf.pos() + len > m_buf.size()) {
            m_valid = false;
            return false;
        }
        auto data_vec = std::vector<nuraft::byte>(len);
        std::memcpy(data_vec.data(), data, len);
        m_buf.put_raw(data_vec.data(), len);
        return true;
    }

    auto nuraft_serializer::read(void* data, size_t len) -> bool {
        if(m_buf.pos() + len > m_buf.size()) {
            m_valid = false;
            return false;
        }
        std::memcpy(data, m_buf.get_raw(len), len);
        return true;
    }
}
