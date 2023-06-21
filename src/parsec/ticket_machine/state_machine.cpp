// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "state_machine.hpp"

#include "util/rpc/format.hpp"
#include "util/serialization/format.hpp"

namespace cbdc::parsec::ticket_machine {
    state_machine::state_machine(std::shared_ptr<logging::log> logger,
                                 ticket_number_type batch_size)
        : m_logger(std::move(logger)) {
        register_handler_callback([&](rpc::request req) {
            return process_request(req);
        });
        m_ticket_machine = std::make_unique<impl>(m_logger, batch_size);
    }

    auto state_machine::commit(uint64_t log_idx, nuraft::buffer& data)
        -> nuraft::ptr<nuraft::buffer> {
        m_last_committed_idx = log_idx;

        auto resp = blocking_call(data);
        if(!resp.has_value()) {
            // TODO: This would only happen if there was a deserialization
            // error with the request. Maybe we should abort here as such an
            // event would imply a bug in the coordinator.
            return nullptr;
        }

        return resp.value();
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

    auto state_machine::process_request(rpc::request /* req */)
        -> rpc::response {
        auto ret = rpc::response();
        [[maybe_unused]] auto success = m_ticket_machine->get_ticket_number(
            [&](interface::get_ticket_number_return_type tkts) {
                ret = tkts;
            });
        assert(success);
        return ret;
    }
}
