// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "replicated_shard_client.hpp"

#include "format.hpp"
#include "messages.hpp"
#include "util/raft/util.hpp"
#include "util/serialization/format.hpp"

namespace cbdc::parsec::runtime_locking_shard {
    replicated_shard_client::replicated_shard_client(
        std::shared_ptr<raft::node> raft_node)
        : m_raft(std::move(raft_node)) {}

    auto replicated_shard_client::prepare(ticket_number_type ticket_number,
                                          broker_id_type broker_id,
                                          state_type state_update,
                                          callback_type result_callback)
        -> bool {
        auto req = rpc::replicated_prepare_request{ticket_number,
                                                   broker_id,
                                                   std::move(state_update)};
        auto success = replicate_request(
            std::move(req),
            [result_callback](
                std::optional<rpc::replicated_response> maybe_res) {
                if(!maybe_res.has_value()) {
                    result_callback(error_code::internal_error);
                    return;
                }
                auto&& res = maybe_res.value();
                assert(std::holds_alternative<
                       replicated_shard_interface::return_type>(res));
                auto&& resp_val
                    = std::get<replicated_shard_interface::return_type>(res);
                result_callback(resp_val);
            });
        return success;
    }

    auto replicated_shard_client::commit(ticket_number_type ticket_number,
                                         callback_type result_callback)
        -> bool {
        auto req = rpc::commit_request{ticket_number};
        auto success = replicate_request(
            req,
            [result_callback](
                std::optional<rpc::replicated_response> maybe_res) {
                if(!maybe_res.has_value()) {
                    result_callback(error_code::internal_error);
                    return;
                }
                auto&& res = maybe_res.value();
                assert(std::holds_alternative<
                       replicated_shard_interface::return_type>(res));
                auto&& resp_val
                    = std::get<replicated_shard_interface::return_type>(res);
                result_callback(resp_val);
            });
        return success;
    }

    auto replicated_shard_client::finish(ticket_number_type ticket_number,
                                         callback_type result_callback)
        -> bool {
        auto req = rpc::finish_request{ticket_number};
        auto success = replicate_request(
            req,
            [result_callback](
                std::optional<rpc::replicated_response> maybe_res) {
                if(!maybe_res.has_value()) {
                    result_callback(error_code::internal_error);
                    return;
                }
                auto&& res = maybe_res.value();
                assert(std::holds_alternative<
                       replicated_shard_interface::return_type>(res));
                auto&& resp_val
                    = std::get<replicated_shard_interface::return_type>(res);
                result_callback(resp_val);
            });
        return success;
    }

    auto replicated_shard_client::get_tickets(
        get_tickets_callback_type result_callback) const -> bool {
        auto req = rpc::replicated_get_tickets_request{};
        auto success = replicate_request(
            req,
            [result_callback](
                std::optional<rpc::replicated_response> maybe_res) {
                if(!maybe_res.has_value()) {
                    result_callback(error_code::internal_error);
                    return;
                }
                auto&& res = maybe_res.value();
                assert(std::holds_alternative<
                       replicated_shard_interface::get_tickets_return_type>(
                    res));
                auto&& resp_val = std::get<
                    replicated_shard_interface::get_tickets_return_type>(res);
                result_callback(std::move(resp_val));
            });
        return success;
    }

    auto replicated_shard_client::replicate_request(
        const rpc::replicated_request& req,
        const std::function<void(std::optional<rpc::replicated_response>)>&
            result_callback) const -> bool {
        if(!m_raft->is_leader()) {
            return false;
        }
        auto req_buf = make_buffer<rpc::replicated_request,
                                   nuraft::ptr<nuraft::buffer>>(req);
        auto success = m_raft->replicate(
            std::move(req_buf),
            [result_callback](raft::result_type& r,
                              nuraft::ptr<std::exception>& err) {
                if(err) {
                    result_callback(std::nullopt);
                    return;
                }

                const auto res = r.get();
                if(!res) {
                    result_callback(std::nullopt);
                    return;
                }

                auto maybe_resp = from_buffer<rpc::replicated_response>(*res);
                assert(maybe_resp.has_value());
                auto&& resp = maybe_resp.value();
                result_callback(std::move(resp));
            });
        return success;
    }
}
