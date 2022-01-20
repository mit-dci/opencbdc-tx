// Copyright (c) 2021 MIT Digital Currency Initiative
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "block.hpp"

namespace cbdc {
    auto cbdc::atomizer::block::operator==(const block& rhs) const -> bool {
        return (rhs.m_height == m_height)
            && (rhs.m_transactions == m_transactions);
    }
}
