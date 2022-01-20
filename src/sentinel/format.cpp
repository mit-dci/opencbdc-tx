// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "format.hpp"

#include "serialization/format.hpp"
#include "transaction/messages.hpp"

namespace cbdc {
    auto operator<<(serializer& packet, const sentinel::response& r)
        -> serializer& {
        return packet << r.m_tx_status << r.m_tx_error;
    }

    auto operator>>(serializer& packet, sentinel::response& r) -> serializer& {
        return packet >> r.m_tx_status >> r.m_tx_error;
    }
}
