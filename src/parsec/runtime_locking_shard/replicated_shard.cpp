// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "replicated_shard.hpp"

namespace cbdc::parsec::runtime_locking_shard {
    auto replicated_shard::prepare(ticket_number_type ticket_number,
                                   broker_id_type broker_id,
                                   state_type state_update,
                                   callback_type result_callback) -> bool {
        auto ret = [&]() {
            std::unique_lock l(m_mut);
            m_tickets.emplace(ticket_number,
                              ticket_type{broker_id,
                                          std::move(state_update),
                                          ticket_state::prepared});
            return std::nullopt;
        }();
        result_callback(ret);
        return true;
    }

    auto replicated_shard::commit(ticket_number_type ticket_number,
                                  callback_type result_callback) -> bool {
        auto ret = [&]() -> return_type {
            std::unique_lock l(m_mut);
            auto it = m_tickets.find(ticket_number);
            if(it == m_tickets.end()) {
                return error_code::unknown_ticket;
            }
            auto& t = it->second;
            t.m_state = ticket_state::committed;
            for(auto&& [k, v] : t.m_state_update) {
                m_state[k] = std::move(v);
            }
            return std::nullopt;
        }();
        result_callback(ret);
        return true;
    }

    auto replicated_shard::finish(ticket_number_type ticket_number,
                                  callback_type result_callback) -> bool {
        auto ret = [&]() -> return_type {
            std::unique_lock l(m_mut);
            m_tickets.erase(ticket_number);
            return std::nullopt;
        }();
        result_callback(ret);
        return true;
    }

    auto replicated_shard::get_tickets(
        get_tickets_callback_type result_callback) const -> bool {
        auto ret = [&]() -> get_tickets_return_type {
            std::unique_lock l(m_mut);
            return m_tickets;
        }();
        result_callback(std::move(ret));
        return true;
    }

    auto replicated_shard::get_state() const -> state_type {
        std::unique_lock l(m_mut);
        return m_state;
    }
}
