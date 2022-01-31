// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ostream_serializer.hpp"

#include <cstring>
#include <vector>

namespace cbdc {
    ostream_serializer::ostream_serializer(std::ostream& s)
        : stream_serializer(s),
          m_str(s) {}

    auto ostream_serializer::end_of_buffer() const -> bool {
        auto current_pos = m_str.tellp();
        m_str.seekp(0, std::ios::end);
        if(m_str.tellp() == current_pos) {
            return true;
        }
        m_str.seekp(current_pos);
        return false;
    }

    void ostream_serializer::advance_cursor(size_t len) {
        m_str.seekp(static_cast<off_type>(len), std::ios::cur);
    }

    void ostream_serializer::reset() {
        m_str.clear();
        m_str.seekp(0);
    }

    auto ostream_serializer::write(const void* data, size_t len) -> bool {
        auto write_vec = std::vector<char>(len);
        std::memcpy(write_vec.data(), data, len);
        return static_cast<bool>(
            m_str.write(write_vec.data(), static_cast<off_type>(len)));
    }

    auto ostream_serializer::read(void* /* data */, size_t /* len */) -> bool {
        m_str.setstate(std::ios::failbit);
        return false;
    }
}
