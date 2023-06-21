// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_RUNTIME_LOCKING_SHARD_CLIENT_H_
#define OPENCBDC_TX_SRC_PARSEC_RUNTIME_LOCKING_SHARD_CLIENT_H_

#include "interface.hpp"
#include "messages.hpp"
#include "util/rpc/tcp_client.hpp"

namespace cbdc::parsec::runtime_locking_shard::rpc {
    /// RPC client for a runtime locking shard cluster.
    class client : public interface {
      public:
        /// Constructor.
        /// \param endpoints RPC server endpoints for the runtime locking shard
        ///                  cluster.
        explicit client(std::vector<network::endpoint_t> endpoints);

        client() = delete;
        ~client() override = default;
        client(const client&) = delete;
        auto operator=(const client&) -> client& = delete;
        client(client&&) = delete;
        auto operator=(client&&) -> client& = delete;

        /// Initializes the underlying TCP client.
        /// \return true if the client initialized successfully.
        auto init() -> bool;

        /// Requests a try lock operation from the remote shard.
        /// \param ticket_number ticket number.
        /// \param broker_id ID of broker managing ticket.
        /// \param key key to lock.
        /// \param locktype type of lock to acquire.
        /// \param first_lock true if this is the first lock.
        /// \param result_callback function to call with try lock result.
        /// \return true if the request was sent successfully.
        auto try_lock(ticket_number_type ticket_number,
                      broker_id_type broker_id,
                      key_type key,
                      lock_type locktype,
                      bool first_lock,
                      try_lock_callback_type result_callback) -> bool override;

        /// Requests a prepare operation from the remote shard.
        /// \param ticket_number ticket number.
        /// \param broker_id ID of broker managing ticket.
        /// \param state_update state updates to apply if ticket is committed.
        /// \param result_callback function to call with prepare result.
        /// \return true if the request was sent successfully.
        auto prepare(ticket_number_type ticket_number,
                     broker_id_type broker_id,
                     state_update_type state_update,
                     prepare_callback_type result_callback) -> bool override;

        /// Requests a commit operation from the remote shard.
        /// \param ticket_number ticket number.
        /// \param result_callback function to call with commit result.
        /// \return true if the request was sent successfully.
        auto commit(ticket_number_type ticket_number,
                    commit_callback_type result_callback) -> bool override;

        /// Requests a rollback operation from the remote shard.
        /// \param ticket_number ticket number.
        /// \param result_callback function to call with the rollback result.
        /// \return true if the request was sent successfully.
        auto rollback(ticket_number_type ticket_number,
                      rollback_callback_type result_callback) -> bool override;

        /// Requests a finish operation from the remote shard.
        /// \param ticket_number ticket number.
        /// \param result_callback function to call with the finish result.
        /// \return true if the request was sent successfully.
        auto finish(ticket_number_type ticket_number,
                    finish_callback_type result_callback) -> bool override;

        /// Requests a get tickets operation from the remote shard.
        /// \param broker_id broker ID.
        /// \param result_callback function to call with the get tickets
        ///                        result.
        /// \return true if the request was sent successfully.
        auto get_tickets(broker_id_type broker_id,
                         get_tickets_callback_type result_callback)
            -> bool override;

      private:
        std::unique_ptr<cbdc::rpc::tcp_client<request, response>> m_client;
    };
}

#endif
