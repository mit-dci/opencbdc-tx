// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "messages.hpp"

#include "util/serialization/format.hpp"
#include "watchtower.hpp"

namespace cbdc {
    auto
    operator<<(cbdc::serializer& packet,
               const cbdc::watchtower::best_block_height_response& bbh_res)
        -> cbdc::serializer& {
        return packet << bbh_res.m_height;
    }

    auto operator>>(cbdc::serializer& packet,
                    cbdc::watchtower::best_block_height_response& bbh_res)
        -> cbdc::serializer& {
        return packet >> bbh_res.m_height;
    }

    auto
    operator<<(cbdc::serializer& packet,
               const cbdc::watchtower::request& req) -> cbdc::serializer& {
        return packet << req.m_req;
    }

    auto
    operator<<(cbdc::serializer& packet,
               const cbdc::watchtower::response& res) -> cbdc::serializer& {
        return packet << res.m_resp;
    }
}
