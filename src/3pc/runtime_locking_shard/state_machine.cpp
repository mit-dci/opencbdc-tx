// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "state_machine.hpp"

#include "format.hpp"
#include "util/raft/util.hpp"
#include "util/rpc/format.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/util.hpp"

namespace cbdc::threepc::runtime_locking_shard {
    auto state_machine::commit(uint64_t log_idx, nuraft::buffer& data)
        -> nuraft::ptr<nuraft::buffer> {
        auto maybe_req = from_buffer<rpc::replicated_request>(data);
        if(!maybe_req.has_value()) {
            // TODO: This would only happen if there was a deserialization
            // error with the request. Maybe we should abort here as such an
            // event would imply a bug in the coordinator.
            return nullptr;
        }

        auto&& req = maybe_req.value();
        auto resp = process_request(req);

        auto resp_buf = make_buffer<rpc::replicated_response,
                                    nuraft::ptr<nuraft::buffer>>(resp);

        m_last_committed_idx = log_idx;

        return resp_buf;
    }

    auto state_machine::apply_snapshot(nuraft::snapshot& /* s */) -> bool {
        return false;
    }

    auto state_machine::last_snapshot() -> nuraft::ptr<nuraft::snapshot> {
        return nullptr;
    }

    auto state_machine::last_commit_index() -> uint64_t {
        return m_last_committed_idx;
    }

    void state_machine::create_snapshot(
        nuraft::snapshot& /* s */,
        nuraft::async_result<bool>::handler_type& when_done) {
        nuraft::ptr<std::exception> except(nullptr);
        bool ret = false;
        when_done(ret, except);
    }

    auto state_machine::process_request(const rpc::replicated_request& req)
        -> rpc::replicated_response {
        auto ret = rpc::replicated_response();
        [[maybe_unused]] auto success = std::visit(
            overloaded{
                [&](const rpc::replicated_prepare_request& msg) {
                    return m_shard->prepare(
                        msg.m_ticket_number,
                        msg.m_broker_id,
                        msg.m_state_update,
                        [&](replicated_shard::return_type res) {
                            ret = res;
                        });
                },
                [&](rpc::commit_request msg) {
                    return m_shard->commit(
                        msg.m_ticket_number,
                        [&](replicated_shard::return_type res) {
                            ret = res;
                        });
                },
                [&](rpc::finish_request msg) {
                    return m_shard->finish(
                        msg.m_ticket_number,
                        [&](replicated_shard::return_type res) {
                            ret = res;
                        });
                },
                [&](rpc::replicated_get_tickets_request /* msg */) {
                    return m_shard->get_tickets(
                        [&](replicated_shard::get_tickets_return_type res) {
                            ret = res;
                        });
                }},
            req);
        assert(success);
        return ret;
    }

    auto state_machine::get_shard() const
        -> std::shared_ptr<replicated_shard> {
        return m_shard;
    }
}
