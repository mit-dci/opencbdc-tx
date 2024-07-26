// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "istream_serializer.hpp"

#include <cstring>
#include <vector>

namespace cbdc {
    istream_serializer::istream_serializer(std::istream& s)
        : stream_serializer(s),
          m_str(s) {}

    auto istream_serializer::end_of_buffer() const -> bool {
        auto current_pos = m_str.tellg();
        m_str.seekg(0, std::ios::end);
        if(m_str.tellg() == current_pos) {
            return true;
        }
        m_str.seekg(current_pos);
        return false;
    }

    void istream_serializer::advance_cursor(size_t len) {
        m_str.seekg(static_cast<off_type>(len), std::ios::cur);
    }

    void istream_serializer::reset() {
        m_str.clear();
        m_str.seekg(0);
    }

    auto istream_serializer::write(const void* /* data */,
                                   size_t /* len */) -> bool {
        m_str.setstate(std::ios::failbit);
        return false;
    }

    auto istream_serializer::read(void* data, size_t len) -> bool {
        auto read_vec = std::vector<char>(len);
        if(!m_str.read(read_vec.data(), static_cast<off_type>(len))) {
            return false;
        }

        std::memcpy(data, read_vec.data(), len);

        return true;
    }
}
