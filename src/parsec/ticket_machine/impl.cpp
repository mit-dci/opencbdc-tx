// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "impl.hpp"

namespace cbdc::parsec::ticket_machine {
    impl::impl(std::shared_ptr<logging::log> logger, ticket_number_type range)
        : m_log(std::move(logger)),
          m_range(range) {}

    auto impl::get_ticket_number(
        get_ticket_number_callback_type result_callback) -> bool {
        auto ticket_number = m_next_ticket_number.fetch_add(m_range);
        result_callback(
            ticket_number_range_type{ticket_number, ticket_number + m_range});
        return true;
    }
}
