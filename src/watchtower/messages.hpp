// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/** \file messages.hpp
 * Messages clients can use to communicate with the Watchtower.
 */

#ifndef OPENCBDC_TX_SRC_WATCHTOWER_MESSAGES_H_
#define OPENCBDC_TX_SRC_WATCHTOWER_MESSAGES_H_

#include "serialization/serializer.hpp"

namespace cbdc::watchtower {
    struct best_block_height_request;
    class best_block_height_response;
    class request;
    class response;
}

namespace cbdc {
    auto
    operator<<(cbdc::serializer& packet,
               const cbdc::watchtower::best_block_height_response& bbh_res)
        -> cbdc::serializer&;
    auto operator>>(cbdc::serializer& packet,
                    cbdc::watchtower::best_block_height_response& bbh_res)
        -> cbdc::serializer&;
    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::response& res)
        -> cbdc::serializer&;
    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::request& req) -> cbdc::serializer&;
}

#endif // OPENCBDC_TX_SRC_WATCHTOWER_MESSAGES_H_
