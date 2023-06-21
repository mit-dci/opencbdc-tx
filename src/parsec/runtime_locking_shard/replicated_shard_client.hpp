// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_RUNTIME_LOCKING_SHARD_REPLICATED_SHARD_CLIENT_H_
#define OPENCBDC_TX_SRC_PARSEC_RUNTIME_LOCKING_SHARD_REPLICATED_SHARD_CLIENT_H_

#include "messages.hpp"
#include "replicated_shard_interface.hpp"
#include "util/raft/node.hpp"

namespace cbdc::parsec::runtime_locking_shard {
    /// Client for asynchronously interacting with a raft replicated shard on
    /// the leader node of the cluster.
    class replicated_shard_client final : public replicated_shard_interface {
      public:
        /// Constructs a shard client.
        /// \param raft_node pointer to the raft node to control.
        explicit replicated_shard_client(
            std::shared_ptr<raft::node> raft_node);

        /// Replicates a prepare request in the state machine and returns the
        /// response via a callback function.
        /// \param ticket_number ticket to prepare.
        /// \param broker_id broker managing the ticket.
        /// \param state_update keys and values to update after commit.
        /// \param result_callback function to call with prepare result.
        /// \return true if request replication was initiated successfully.
        auto prepare(ticket_number_type ticket_number,
                     broker_id_type broker_id,
                     state_type state_update,
                     callback_type result_callback) -> bool override;

        /// Replicates a commit request in the state machine and returns the
        /// response via a callback function.
        /// \param ticket_number ticket to commit.
        /// \param result_callback function to call with commit result.
        /// \return true if request replication was initiated successfully.
        auto commit(ticket_number_type ticket_number,
                    callback_type result_callback) -> bool override;

        /// Replicates a finish request in the state machine and returns the
        /// response via a callback function.
        /// \param ticket_number ticket to finish.
        /// \param result_callback function to call with finish result.
        /// \return true if request replication was initiated successfully.
        auto finish(ticket_number_type ticket_number,
                    callback_type result_callback) -> bool override;

        /// Replicates a get tickets request in the state machine and returns
        /// the response via a callback function.
        /// \param result_callback function to call with the tickets held by
        ///                        the state machine.
        /// \return true if request replication was initiated successfully.
        [[nodiscard]] auto
        get_tickets(get_tickets_callback_type result_callback) const
            -> bool override;

      private:
        std::shared_ptr<raft::node> m_raft;

        auto replicate_request(
            const rpc::replicated_request& req,
            const std::function<void(std::optional<rpc::replicated_response>)>&
                result_callback) const -> bool;
    };
}

#endif
