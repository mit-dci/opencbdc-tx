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

    auto operator<<(serializer& ser, const buffer& b) -> serializer& {
        ser << static_cast<uint64_t>(b.size());
        ser.write(b.data(), b.size());
        return ser;
    }

    auto operator>>(serializer& deser, buffer& b) -> serializer& {
        uint64_t sz{};
        deser >> sz;
        b.extend(sz);
        deser.read(b.data(), sz);
        return deser;
    }
}
