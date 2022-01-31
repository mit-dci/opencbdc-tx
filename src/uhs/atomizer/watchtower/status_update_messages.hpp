// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/** \file status_update_messages.hpp
 * Messages clients can use to request status updates from the watchtower.
 */

#ifndef OPENCBDC_TX_SRC_WATCHTOWER_STATUS_UPDATE_MESSAGES_H_
#define OPENCBDC_TX_SRC_WATCHTOWER_STATUS_UPDATE_MESSAGES_H_

#include "util/serialization/serializer.hpp"

namespace cbdc {
    namespace watchtower {
        class status_update_request;
        class status_update_state;
        class status_request_check_success;
    }
    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::status_update_request& su_req)
        -> cbdc::serializer&;
    auto operator>>(cbdc::serializer& packet,
                    cbdc::watchtower::status_update_request& su_req)
        -> cbdc::serializer&;
    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::status_update_state& state)
        -> cbdc::serializer&;
    auto operator>>(cbdc::serializer& packet,
                    cbdc::watchtower::status_update_state& state)
        -> cbdc::serializer&;
    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::status_request_check_success& chs)
        -> cbdc::serializer&;
    auto operator>>(cbdc::serializer& packet,
                    cbdc::watchtower::status_request_check_success& chs)
        -> cbdc::serializer&;
}

#endif // OPENCBDC_TX_SRC_WATCHTOWER_STATUS_UPDATE_MESSAGES_H_
