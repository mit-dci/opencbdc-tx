// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COORDINATOR_FORMAT_H_
#define OPENCBDC_TX_SRC_COORDINATOR_FORMAT_H_

#include "controller.hpp"
#include "state_machine.hpp"

namespace cbdc {
    auto operator<<(serializer& ser,
                    const coordinator::state_machine::coordinator_state& s)
        -> serializer&;
    auto
    operator>>(serializer& deser,
               coordinator::controller::coordinator_state& s) -> serializer&;

    auto
    operator<<(serializer& ser,
               const coordinator::controller::sm_command& c) -> serializer&;

    auto operator<<(serializer& ser,
                    const coordinator::controller::sm_command_header& c)
        -> serializer&;
    auto
    operator>>(serializer& deser,
               coordinator::controller::sm_command_header& c) -> serializer&;
}

#endif // OPENCBDC_TX_SRC_COORDINATOR_FORMAT_H_
