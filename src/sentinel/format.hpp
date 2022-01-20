// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SENTINEL_FORMAT_H_
#define OPENCBDC_TX_SRC_SENTINEL_FORMAT_H_

#include "interface.hpp"

namespace cbdc {
    auto operator<<(serializer& packet, const sentinel::response& r)
        -> serializer&;
    auto operator>>(serializer& packet, sentinel::response& r) -> serializer&;
}

#endif // OPENCBDC_TX_SRC_SENTINEL_FORMAT_H_
