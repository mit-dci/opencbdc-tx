// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "format.hpp"

#include "uhs/transaction/messages.hpp"
#include "util/serialization/format.hpp"

namespace cbdc {
    auto operator<<(serializer& packet, const sentinel::response& r)
        -> serializer& {
        return packet << r.m_tx_status << r.m_tx_error;
    }

    auto operator>>(serializer& packet, sentinel::response& r) -> serializer& {
        return packet >> r.m_tx_status >> r.m_tx_error;
    }
}
