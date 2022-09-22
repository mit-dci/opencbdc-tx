// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CBDC_UNIVERSE0_SRC_3PC_BROKER_INTERFACE_H_
#define CBDC_UNIVERSE0_SRC_3PC_BROKER_INTERFACE_H_

#include "3pc/runtime_locking_shard/interface.hpp"
#include "3pc/ticket_machine/interface.hpp"

namespace cbdc::threepc::broker {
    /// Ticket number type.
    using ticket_number_type = ticket_machine::ticket_number_type;
    /// Shard key type.
    using key_type = runtime_locking_shard::key_type;
    /// Shard value type.
    using value_type = runtime_locking_shard::value_type;
    /// Shard state updates type.
    using state_update_type = runtime_locking_shard::state_update_type;
    /// Shard lock type.
    using lock_type = runtime_locking_shard::lock_type;
    /// Set of held locks
    using held_locks_set_type = std::
        unordered_map<key_type, lock_type, hashing::const_sip_hash<key_type>>;

    /// Interface for a broker. Abstracts and simplified the three-phase commit
    /// protocol between multiple shards so that they behave as if there is
    /// only one shard. Handles recovery of tickets managed by a particular
    /// broker instance if the instance fails while tickets are in flight.
    class interface {
      public:
        virtual ~interface() = default;

        interface() = default;
        interface(const interface&) = delete;
        auto operator=(const interface&) -> interface& = delete;
        interface(interface&&) = delete;
        auto operator=(interface&&) -> interface& = delete;

        /// Error codes returned by broker operations.
        enum class error_code : uint8_t {
            /// Error during ticket number assignment.
            ticket_number_assignment,
            /// Request for an unknown ticket.
            unknown_ticket,
            /// Request invalid because ticket is prepared.
            prepared,
            /// Request failed because a shard was unreachable.
            shard_unreachable,
            /// Request failed because the ticket machine was unreachable.
            ticket_machine_unreachable,
            /// Reqeust invalid because ticket is committed.
            committed,
            /// Request invalid because ticket is not prepared.
            not_prepared,
            /// Request invalid because ticket is not committed or rolled
            /// back.
            begun,
            /// Request invalid because ticket is rolled back.
            aborted,
            /// Request failed because directory was unreachable.
            directory_unreachable,
            /// Request failed because shard was in an invalid state for the
            /// given ticket.
            invalid_shard_state,
            /// Cannot prepare because ticket still waiting for locks to be
            /// acquired.
            waiting_for_locks,
            /// Shard error during commit.
            commit_error,
            /// Shard error during rollback.
            rollback_error,
            /// Shard error during prepare.
            prepare_error,
            /// Shard error during finish.
            finish_error,
            /// Shard error during get tickets.
            get_tickets_error
        };

        /// Return type from a begin operation. Either a new ticket number or
        /// an error code.
        using begin_return_type = std::variant<ticket_number_type, error_code>;
        /// Callback function type for a begin operation.
        using begin_callback_type = std::function<void(begin_return_type)>;

        /// Acquires a new ticket number to begin a transaction.
        /// \param result_callback function to call with begin result.
        /// \return true if the operation was initiated successfully.
        [[nodiscard]] virtual auto begin(begin_callback_type result_callback)
            -> bool
            = 0;

        /// Return type from a try lock operation. Either the value associated
        /// with the requested key, a broker error, or a shard error.
        using try_lock_return_type
            = std::variant<value_type,
                           error_code,
                           runtime_locking_shard::shard_error>;
        /// Callback function type for a try lock operation.
        using try_lock_callback_type
            = std::function<void(try_lock_return_type)>;

        /// Attempts to acquire the given lock on the appropriate shard.
        /// \param ticket_number ticket number.
        /// \param key key to lock.
        /// \param locktype type of lock to acquire.
        /// \param result_callback function to call with try lock result.
        /// \return true if the operation was initiated successfully.
        [[nodiscard]] virtual auto
        try_lock(ticket_number_type ticket_number,
                 key_type key,
                 lock_type locktype,
                 try_lock_callback_type result_callback) -> bool
            = 0;

        /// Return type from a commit operation. Broker or shard error code, if
        /// applicable.
        using commit_return_type = std::optional<
            std::variant<error_code, runtime_locking_shard::shard_error>>;
        /// Callback function type for a commit operation.
        using commit_callback_type = std::function<void(commit_return_type)>;

        /// Prepares and commits a ticket on all shards involved in the ticket.
        /// \param ticket_number ticket number.
        /// \param state_updates state updates to commit.
        /// \param result_callback function to call with commit result.
        /// \return true if the operation was initiated successfully.
        [[nodiscard]] virtual auto commit(ticket_number_type ticket_number,
                                          state_update_type state_updates,
                                          commit_callback_type result_callback)
            -> bool
            = 0;

        /// Return type from a finish operation. Broker error code, if
        /// applicable.
        using finish_return_type = std::optional<error_code>;
        /// Callback function type for a finish operation.
        using finish_callback_type = std::function<void(finish_return_type)>;

        /// Finishes a ticket on all shards involved in the ticket.
        /// \param ticket_number ticket number.
        /// \param result_callback function to call with finish result.
        /// \return true if the operation was initiated successfully.
        [[nodiscard]] virtual auto finish(ticket_number_type ticket_number,
                                          finish_callback_type result_callback)
            -> bool
            = 0;

        /// Return type from a rollback operation. Broker or shard error code,
        /// if applicable.
        using rollback_return_type = std::optional<
            std::variant<error_code, runtime_locking_shard::error_code>>;
        /// Callback function type for a rollback operation.
        using rollback_callback_type
            = std::function<void(rollback_return_type)>;

        /// Rollback a ticket on all shards involved in the ticket.
        /// \param ticket_number ticket number.
        /// \param result_callback function to call with rollback result.
        /// \return true if the operation was initiated successfully.
        [[nodiscard]] virtual auto
        rollback(ticket_number_type ticket_number,
                 rollback_callback_type result_callback) -> bool
            = 0;

        /// Return type from a recover operation. Broker error code, if
        /// applicable.
        using recover_return_type = std::optional<error_code>;
        /// Callback function type for a recovery operation.
        using recover_callback_type = std::function<void(recover_return_type)>;

        /// Retrieves tickets associated with this broker from all shards and
        /// completes partially committed tickets, and rolls back uncommitted
        /// tickets. Finishes all tickets.
        /// \param result_callback function to call with recovery result.
        /// \return true if the operation was initated successfully.
        [[nodiscard]] virtual auto
        recover(recover_callback_type result_callback) -> bool
            = 0;
    };
}

#endif
