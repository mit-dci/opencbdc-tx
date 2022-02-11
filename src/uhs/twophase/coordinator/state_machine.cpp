// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "state_machine.hpp"

#include "controller.hpp"
#include "format.hpp"
#include "util/raft/serialization.hpp"
#include "util/serialization/util.hpp"

namespace cbdc::coordinator {
    auto state_machine::commit(uint64_t log_idx, nuraft::buffer& data)
        -> nuraft::ptr<nuraft::buffer> {
        m_last_committed_idx = log_idx;
        auto comm = cbdc::coordinator::controller::sm_command_header();
        auto deser = cbdc::nuraft_serializer(data);
        // Deserialize the header from the state machine command
        deser >> comm;
        switch(comm.m_comm) {
            case command::prepare: {
                // Put the dtx in the prepare phase and associated data in the
                // relevant map
                auto res = m_state.m_prepare_txs.emplace(
                    comm.m_dtx_id.value(),
                    nuraft::buffer::copy(data));
                if(!res.second) {
                    // dtx IDs are supposed to be unique so if the dtx is
                    // already present in the prepare map, that would indicate
                    // a bug elsewhere in the codebase. Crash to protect the
                    // system.
                    m_logger->fatal("Duplicate prepare for dtx",
                                    to_string(comm.m_dtx_id.value()));
                }
                break;
            }
            case command::commit: {
                // Remove the dtx from the prepare map
                auto res = m_state.m_prepare_txs.erase(comm.m_dtx_id.value());
                if(res == 0U) {
                    // To be in the commit phase the dtx should have been in
                    // the prepare phase. If it's not, that's a bug and we
                    // crash to protect the system.
                    m_logger->fatal("Prepare not found for commit dtx",
                                    to_string(comm.m_dtx_id.value()));
                }
                // Put the dtx in the commit map with associated data
                m_state.m_commit_txs.emplace(comm.m_dtx_id.value(),
                                             nuraft::buffer::copy(data));

                break;
            }
            case command::discard: {
                // Remove the dtx from the commit map
                auto res = m_state.m_commit_txs.erase(comm.m_dtx_id.value());
                if(res == 0U) {
                    // If the dtx wasn't in the commit map a bug has occurred
                    // and we crash
                    m_logger->fatal("Commit not found for discard dtx",
                                    to_string(comm.m_dtx_id.value()));
                }
                // Add the dtx to the discard set
                m_state.m_discard_txs.emplace(comm.m_dtx_id.value());
                break;
            }
            case command::done: {
                // Remove the dtx from the discard map
                auto res = m_state.m_discard_txs.erase(comm.m_dtx_id.value());
                if(res == 0U) {
                    // The dtx must have been discarded to be considered done.
                    // If it wasn't in the map, crash.
                    m_logger->fatal("Discard not found for done dtx",
                                    to_string(comm.m_dtx_id.value()));
                }
                break;
            }
            case command::get: {
                // Retrieve and serialize the current coordinator state to send
                // back to the requester
                auto ret
                    = nuraft::buffer::alloc(cbdc::serialized_size(m_state));
                auto ser = cbdc::nuraft_serializer(*ret);
                ser << m_state;
                // Sanity check to ensure ret_sz is correct
                assert(ser.end_of_buffer());
                return ret;
            }
        }
        return nullptr;
    }

    auto state_machine::apply_snapshot(nuraft::snapshot& /* s */) -> bool {
        // TODO: implement snapshots (cf. #768)
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
        bool res{false};
        auto snp = nuraft::ptr<std::exception>();
        when_done(res, snp);
    }

    state_machine::state_machine(std::shared_ptr<logging::log> logger)
        : m_logger(std::move(logger)) {}
}
