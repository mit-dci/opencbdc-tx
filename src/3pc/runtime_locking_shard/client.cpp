// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "client.hpp"

#include "format.hpp"
#include "util/serialization/format.hpp"

namespace cbdc::threepc::runtime_locking_shard::rpc {
    client::client(std::vector<network::endpoint_t> endpoints)
        : m_client(std::make_unique<decltype(m_client)::element_type>(
            std::move(endpoints))) {}

    auto client::init() -> bool {
        return m_client->init();
    }

    auto client::try_lock(ticket_number_type ticket_number,
                          broker_id_type broker_id,
                          key_type key,
                          lock_type locktype,
                          bool first_lock,
                          try_lock_callback_type result_callback) -> bool {
        auto req = try_lock_request{ticket_number,
                                    broker_id,
                                    key,
                                    locktype,
                                    first_lock};
        return m_client->call(
            std::move(req),
            [result_callback](std::optional<response> resp) {
                assert(resp.has_value());
                assert(std::holds_alternative<try_lock_return_type>(
                    resp.value()));
                result_callback(std::get<try_lock_return_type>(resp.value()));
            });
    }

    auto client::prepare(ticket_number_type ticket_number,
                         broker_id_type broker_id,
                         state_update_type state_update,
                         prepare_callback_type result_callback) -> bool {
        auto req = prepare_request{ticket_number, state_update, broker_id};
        return m_client->call(
            std::move(req),
            [result_callback](std::optional<response> resp) {
                assert(resp.has_value());
                assert(
                    std::holds_alternative<prepare_return_type>(resp.value()));
                result_callback(std::get<prepare_return_type>(resp.value()));
            });
    }

    auto client::commit(ticket_number_type ticket_number,
                        commit_callback_type result_callback) -> bool {
        auto req = commit_request{ticket_number};
        return m_client->call(
            req,
            [result_callback](std::optional<response> resp) {
                assert(resp.has_value());
                assert(
                    std::holds_alternative<commit_return_type>(resp.value()));
                result_callback(std::get<commit_return_type>(resp.value()));
            });
    }

    auto client::rollback(ticket_number_type ticket_number,
                          rollback_callback_type result_callback) -> bool {
        auto req = rollback_request{ticket_number};
        return m_client->call(
            req,
            [result_callback](std::optional<response> resp) {
                assert(resp.has_value());
                assert(std::holds_alternative<rollback_return_type>(
                    resp.value()));
                result_callback(std::get<rollback_return_type>(resp.value()));
            });
    }

    auto client::finish(ticket_number_type ticket_number,
                        finish_callback_type result_callback) -> bool {
        auto req = finish_request{ticket_number};
        return m_client->call(
            req,
            [result_callback](std::optional<response> resp) {
                assert(resp.has_value());
                assert(
                    std::holds_alternative<finish_return_type>(resp.value()));
                result_callback(std::get<finish_return_type>(resp.value()));
            });
    }

    auto client::get_tickets(broker_id_type broker_id,
                             get_tickets_callback_type result_callback)
        -> bool {
        auto req = get_tickets_request{broker_id};
        return m_client->call(
            req,
            [result_callback](std::optional<response> resp) {
                assert(resp.has_value());
                assert(std::holds_alternative<get_tickets_return_type>(
                    resp.value()));
                result_callback(
                    std::get<get_tickets_return_type>(resp.value()));
            });
    }
}
