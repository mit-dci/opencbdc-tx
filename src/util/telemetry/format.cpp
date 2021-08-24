// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "format.hpp"

#include "util/serialization/format.hpp"

namespace cbdc {
    auto operator<<(serializer& packet, std::string s) -> serializer& {
        std::vector<char> v(s.begin(), s.end());
        packet << v;
        return packet;
    }

    auto operator>>(serializer& packet, std::string& s) -> serializer& {
        std::vector<char> v;
        packet >> v;
        std::string s2(v.begin(), v.end());
        s = s2;
        return packet;
    }
}
