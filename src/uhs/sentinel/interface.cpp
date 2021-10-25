// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "interface.hpp"

namespace cbdc::sentinel {
    auto execute_response::operator==(
        const cbdc::sentinel::execute_response& rhs) const -> bool {
        return std::tie(m_tx_status, m_tx_error)
            == std::tie(rhs.m_tx_status, rhs.m_tx_error);
    }

    auto to_string(tx_status status) -> std::string {
        auto ret = std::string();
        switch(status) {
            case cbdc::sentinel::tx_status::state_invalid:
                ret = "Contextually invalid";
                break;
            case cbdc::sentinel::tx_status::confirmed:
                ret = "Confirmed";
                break;
            case cbdc::sentinel::tx_status::pending:
                ret = "Pending";
                break;
            case cbdc::sentinel::tx_status::static_invalid:
                ret = "Statically invalid";
                break;
        }
        return ret;
    }
}
