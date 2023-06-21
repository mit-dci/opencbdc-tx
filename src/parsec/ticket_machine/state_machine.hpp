// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_TICKET_MACHINE_STATE_MACHINE_H_
#define OPENCBDC_TX_SRC_PARSEC_TICKET_MACHINE_STATE_MACHINE_H_

#include "impl.hpp"
#include "messages.hpp"
#include "util/common/logging.hpp"
#include "util/rpc/blocking_server.hpp"

#include <libnuraft/nuraft.hxx>

namespace cbdc::parsec::ticket_machine {
    /// NuRaft state machine implementation for a replicated ticket machine.
    class state_machine
        : public nuraft::state_machine,
          public cbdc::rpc::blocking_server<rpc::request,
                                            rpc::response,
                                            nuraft::buffer&,
                                            nuraft::ptr<nuraft::buffer>> {
      public:
        /// Constructor.
        /// \param logger log instance.
        /// \param batch_size number of ticket numbers to return per request.
        state_machine(std::shared_ptr<logging::log> logger,
                      ticket_number_type batch_size);

        /// Commit the given raft log entry at the given log index, and return
        /// the result.
        /// \param log_idx raft log index of the log entry.
        /// \param data serialized RPC request.
        /// \return serialized RPC response or nullptr if there was an error
        ///         processing the request.
        auto commit(uint64_t log_idx, nuraft::buffer& data)
            -> nuraft::ptr<nuraft::buffer> override;

        /// Not implemented for ticket machine.
        /// \return false.
        auto apply_snapshot(nuraft::snapshot& /* s */) -> bool override;

        /// Not implemented for ticket machine.
        /// \return nullptr.
        auto last_snapshot() -> nuraft::ptr<nuraft::snapshot> override;

        /// Returns the most recently committed log entry index.
        /// \return log entry index.
        auto last_commit_index() -> uint64_t override;

        /// Not implemented for ticket machine.
        void create_snapshot(
            nuraft::snapshot& /* s */,
            nuraft::async_result<bool>::handler_type& /* when_done */)
            override;

      private:
        auto process_request(rpc::request req) -> rpc::response;

        std::atomic<uint64_t> m_last_committed_idx{0};

        std::unique_ptr<impl> m_ticket_machine{};

        std::shared_ptr<logging::log> m_logger;
    };
}

#endif
