// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "messages.hpp"

#include <tuple>

namespace cbdc::locking_shard::rpc {
    auto request::operator==(const request& rhs) const -> bool {
        return std::tie(m_dtx_id, m_params)
            == std::tie(rhs.m_dtx_id, rhs.m_params);
    }
}
