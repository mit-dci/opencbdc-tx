// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "serialization/format.hpp"
#include "status_update.hpp"

namespace cbdc {
    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::status_update_request& su_req)
        -> cbdc::serializer& {
        return packet << su_req.m_uhs_ids;
    }

    auto operator>>(cbdc::serializer& packet,
                    cbdc::watchtower::status_update_request& su_req)
        -> cbdc::serializer& {
        return packet >> su_req.m_uhs_ids;
    }

    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::status_update_state& state)
        -> cbdc::serializer& {
        return packet << static_cast<uint32_t>(state.m_status)
                      << state.m_block_height << state.m_uhs_id;
    }

    auto operator>>(cbdc::serializer& packet,
                    cbdc::watchtower::status_update_state& state)
        -> cbdc::serializer& {
        uint32_t status{};
        packet >> status >> state.m_block_height >> state.m_uhs_id;
        state.m_status = static_cast<cbdc::watchtower::search_status>(status);
        return packet;
    }

    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::status_request_check_success& chs)
        -> cbdc::serializer& {
        return packet << chs.m_states;
    }

    auto operator>>(cbdc::serializer& packet,
                    cbdc::watchtower::status_request_check_success& chs)
        -> cbdc::serializer& {
        return packet >> chs.m_states;
    }
}
