// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_RUNTIME_LOCKING_SHARD_IMPL_H_
#define OPENCBDC_TX_SRC_3PC_RUNTIME_LOCKING_SHARD_IMPL_H_

#include "interface.hpp"
#include "replicated_shard.hpp"
#include "util/common/hashmap.hpp"
#include "util/common/logging.hpp"

#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace cbdc::threepc::runtime_locking_shard {
    /// Implementation of a runtime locking shard. Stores keys in memory using
    /// a hash map. Thread-safe.
    class impl : public interface {
      public:
        /// Constructor.
        /// \param logger log instance.
        explicit impl(std::shared_ptr<logging::log> logger);

        /// Locks the given key for a ticket and returns the associated value.
        /// If lock is unavailable, lock will be queued. May wound other
        /// tickets to acquire the lock.
        /// \param ticket_number ticket number.
        /// \param broker_id ID of broker managing ticket.
        /// \param key key to lock.
        /// \param locktype type of lock to acquire.
        /// \param first_lock true if this is the first lock.
        /// \param result_callback function to call with try lock result.
        /// \return true.
        auto try_lock(ticket_number_type ticket_number,
                      broker_id_type broker_id,
                      key_type key,
                      lock_type locktype,
                      bool first_lock,
                      try_lock_callback_type result_callback) -> bool override;

        /// Prepares a ticket with the given state updates.
        /// \param ticket_number ticket number.
        /// \param broker_id ID of broker managing ticket.
        /// \param state_update state changes to apply if ticket is committed.
        /// \param result_callback function to call with prepare result.
        /// \return true.
        auto prepare(ticket_number_type ticket_number,
                     broker_id_type broker_id,
                     state_update_type state_update,
                     prepare_callback_type result_callback) -> bool override;

        /// Commits a previously prepared ticket. Releases any locks held by
        /// the ticket and assigns the locks to tickets queuing for the lock.
        /// \param ticket_number ticket number.
        /// \param result_callback function to call with commit result.
        /// \return true.
        auto commit(ticket_number_type ticket_number,
                    commit_callback_type result_callback) -> bool override;

        /// Rolls back an uncommitted ticket. Releases any locks held by
        /// the ticket and assigns the locks to tickets queuing for the lock.
        /// \param ticket_number ticket number.
        /// \param result_callback function to call with rollback result.
        /// \return true.
        auto rollback(ticket_number_type ticket_number,
                      rollback_callback_type result_callback) -> bool override;

        /// Deletes a committed or rolled-back ticket.
        /// \param ticket_number ticket number.
        /// \param result_callback function to call with finish result.
        /// \return true.
        auto finish(ticket_number_type ticket_number,
                    finish_callback_type result_callback) -> bool override;

        /// Returns tickets managed by the given broker.
        /// \param broker_id broker ID.
        /// \param result_callback function to call with get_tickets result.
        /// \return true.
        auto get_tickets(broker_id_type broker_id,
                         get_tickets_callback_type result_callback)
            -> bool override;

        /// Restores the state of another shard instance.
        /// \param state keys and values to store.
        /// \param tickets unfinished tickets that have reached the prepare or
        ///                commit phase.
        /// \return true if the provided state was applied successfully.
        auto recover(const replicated_shard::state_type& state,
                     const replicated_shard::tickets_type& tickets) -> bool;

      private:
        struct lock_queue_element_type {
            lock_type m_type;
            try_lock_callback_type m_callback;
        };

        struct rw_lock_type {
            std::optional<ticket_number_type> m_writer;
            std::unordered_set<ticket_number_type> m_readers;
            std::map<ticket_number_type, lock_queue_element_type> m_queue;
        };

        struct state_element_type {
            value_type m_value;
            rw_lock_type m_lock;
        };

        using key_set_type
            = std::unordered_set<key_type, hashing::const_sip_hash<key_type>>;

        struct ticket_state_type {
            ticket_state m_state{ticket_state::begun};
            std::unordered_map<key_type,
                               lock_type,
                               hashing::const_sip_hash<key_type>>
                m_locks_held;
            key_set_type m_queued_locks;
            state_update_type m_state_update;
            broker_id_type m_broker_id{};
            std::optional<wounded_details> m_wounded_details{};
        };

        struct pending_callback_element_type {
            try_lock_callback_type m_callback;
            try_lock_return_type m_returning;
            ticket_number_type m_ticket_number;
        };

        using pending_callbacks_list_type
            = std::vector<pending_callback_element_type>;

        mutable std::mutex m_mut;
        std::shared_ptr<logging::log> m_log;

        std::unordered_map<key_type,
                           state_element_type,
                           hashing::const_sip_hash<key_type>>
            m_state;
        std::unordered_map<ticket_number_type, ticket_state_type> m_tickets;

        auto
        wound_tickets(key_type key,
                      const std::vector<ticket_number_type>& blocking_tickets,
                      ticket_number_type blocked_ticket)
            -> pending_callbacks_list_type;

        static auto get_waiting_on(ticket_number_type ticket_number,
                                   lock_type locktype,
                                   rw_lock_type& lock)
            -> std::vector<ticket_number_type>;

        auto release_locks(ticket_number_type ticket_number,
                           ticket_state_type& ticket)
            -> std::pair<pending_callbacks_list_type, key_set_type>;

        auto acquire_locks(const key_set_type& keys)
            -> pending_callbacks_list_type;

        auto acquire_lock(const key_type& key,
                          pending_callbacks_list_type& callbacks) -> bool;
    };
}

#endif
