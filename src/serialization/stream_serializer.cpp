// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stream_serializer.hpp"

namespace cbdc {
    stream_serializer::stream_serializer(std::ios& s) : m_str(s) {}

    stream_serializer::operator bool() const {
        return m_str.good();
    }
}
