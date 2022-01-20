// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "state_machine.hpp"

#include "format.hpp"
#include "raft/serialization.hpp"
#include "serialization/format.hpp"

#include <unistd.h>

namespace cbdc::locking_shard {
    state_machine::state_machine(
        const std::pair<uint8_t, uint8_t>& output_range,
        std::shared_ptr<logging::log> logger,
        size_t completed_txs_cache_size,
        const std::string& preseed_file)
        : m_output_range(output_range),
          m_logger(std::move(logger)) {
        register_handler_callback([&](rpc::request req) {
            return process_request(std::move(req));
        });
        m_shard = std::make_unique<locking_shard>(output_range,
                                                  m_logger,
                                                  completed_txs_cache_size,
                                                  preseed_file);
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

    auto state_machine::get_shard_instance()
        -> std::shared_ptr<cbdc::locking_shard::locking_shard> {
        return m_shard;
    }

    auto state_machine::process_request(cbdc::locking_shard::rpc::request req)
        -> cbdc::locking_shard::rpc::response {
        auto dtxid_str = to_string(req.m_dtx_id);
        return std::visit(
            overloaded{[&](rpc::lock_params&& params)
                           -> cbdc::locking_shard::rpc::response {
                           m_logger->info("Processing lock",
                                          dtxid_str,
                                          "with",
                                          params.size(),
                                          "txs");
                           auto res = m_shard->lock_outputs(std::move(params),
                                                            req.m_dtx_id);
                           assert(res.has_value());
                           m_logger->info("Done lock", dtxid_str);
                           return res.value();
                       },
                       [&](rpc::apply_params&& params)
                           -> cbdc::locking_shard::rpc::response {
                           m_logger->info("Processing apply", dtxid_str);
                           [[maybe_unused]] auto res
                               = m_shard->apply_outputs(std::move(params),
                                                        req.m_dtx_id);
                           assert(res);
                           m_logger->info("Done apply", dtxid_str);
                           return rpc::apply_response();
                       },
                       [&](rpc::discard_params&& /* params */)
                           -> cbdc::locking_shard::rpc::response {
                           m_logger->info("Processing discard", dtxid_str);
                           [[maybe_unused]] auto res
                               = m_shard->discard_dtx(req.m_dtx_id);
                           assert(res);
                           m_logger->info("Done discard", dtxid_str);
                           return rpc::discard_response();
                       }},
            std::move(req.m_params));
    }
}
