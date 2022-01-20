// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "format.hpp"

namespace cbdc {
    auto operator<<(serializer& packet, std::byte b) -> serializer& {
        packet << static_cast<uint8_t>(b);
        return packet;
    }

    auto operator>>(serializer& packet, std::byte& b) -> serializer& {
        uint8_t val{};

        if(packet >> val) {
            b = static_cast<std::byte>(val);
        }

        return packet;
    }
}
