// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "client.hpp"

#include "util/serialization/format.hpp"

namespace cbdc::threepc::ticket_machine::rpc {
    client::client(std::vector<network::endpoint_t> endpoints)
        : m_client(std::make_unique<decltype(m_client)::element_type>(
            std::move(endpoints))) {}

    auto client::init() -> bool {
        return m_client->init();
    }

    auto
    client::get_ticket_number(get_ticket_number_callback_type result_callback)
        -> bool {
        auto num = ticket_number_type{};
        {
            std::unique_lock l(m_mut);
            constexpr auto fetch_threshold = 500;
            if(m_tickets.size() < fetch_threshold && !m_fetching_tickets) {
                auto res = fetch_tickets();
                if(res) {
                    m_fetching_tickets = true;
                }
                if(m_tickets.empty() && !res) {
                    return false;
                }
            }
            if(m_tickets.empty()) {
                m_callbacks.emplace(std::move(result_callback));
                return true;
            }

            num = m_tickets.front();
            m_tickets.pop();
        }

        result_callback(ticket_number_range_type{num, num});
        return true;
    }

    auto client::fetch_tickets() -> bool {
        return m_client->call(
            std::monostate{},
            [this](std::optional<get_ticket_number_return_type> res) {
                assert(res.has_value());
                std::visit(overloaded{[&](ticket_number_range_type range) {
                                          handle_ticket_numbers(range);
                                      },
                                      [&](error_code e) {
                                          auto callbacks
                                              = decltype(m_callbacks)();
                                          {
                                              std::unique_lock ll(m_mut);
                                              m_fetching_tickets = false;
                                              callbacks.swap(m_callbacks);
                                          }
                                          while(!callbacks.empty()) {
                                              callbacks.front()(e);
                                              callbacks.pop();
                                          }
                                      }},
                           res.value());
            });
    }

    void client::handle_ticket_numbers(ticket_number_range_type range) {
        auto callbacks = decltype(m_callbacks)();
        auto tickets = decltype(m_tickets)();
        {
            std::unique_lock ll(m_mut);
            for(ticket_number_type i = range.first; i < range.second; i++) {
                m_tickets.push(i);
            }
            while(!m_tickets.empty() && !m_callbacks.empty()) {
                callbacks.push(std::move(m_callbacks.front()));
                m_callbacks.pop();
                tickets.push(m_tickets.front());
                m_tickets.pop();
            }
            if(m_callbacks.empty()) {
                m_fetching_tickets = false;
            } else {
                auto fetch_res = fetch_tickets();
                if(!fetch_res) {
                    m_fetching_tickets = false;
                }
            }
        }
        while(!callbacks.empty()) {
            assert(!tickets.empty());
            callbacks.front()(
                ticket_number_range_type{tickets.front(), tickets.front()});
            callbacks.pop();
            tickets.pop();
        }
    }
}
