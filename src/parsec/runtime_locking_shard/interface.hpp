// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_RUNTIME_LOCKING_SHARD_INTERFACE_H_
#define OPENCBDC_TX_SRC_PARSEC_RUNTIME_LOCKING_SHARD_INTERFACE_H_

#include "parsec/ticket_machine/interface.hpp"
#include "util/common/buffer.hpp"
#include "util/common/hashmap.hpp"

#include <functional>
#include <unordered_map>

namespace cbdc::parsec::runtime_locking_shard {
    /// Type for a ticket number.
    using ticket_number_type = parsec::ticket_machine::ticket_number_type;
    /// Type for keys held by shards.
    using key_type = buffer;
    /// Type for values held by shards.
    using value_type = buffer;
    /// Type for the ID of a broker.
    using broker_id_type = size_t;

    /// Type for state updates to a shard. A map of keys and their new values.
    using state_update_type = std::
        unordered_map<key_type, value_type, hashing::const_sip_hash<key_type>>;

    /// Types of key lock supported by shards.
    enum class lock_type : uint8_t {
        /// Read lock. Multiple readers can hold a lock for the same key.
        read = 0,
        /// Write lock. Only one ticket can hold this lock at a time.
        write = 1,
    };

    /// Error codes returned by methods on shards.
    enum class error_code : uint8_t {
        /// Request invalid because ticket is in the prepared state.
        prepared,
        /// Request invalid because ticket is in the wounded state.
        wounded,
        /// The ticket already holds the requested lock.
        lock_held,
        /// The requested lock is already queued for the given ticket.
        lock_queued,
        /// The given ticket number is not known to this shard.
        unknown_ticket,
        /// Cannot apply requested state update because the ticket does not
        /// hold a write lock on the given key.
        lock_not_held,
        /// Cannot apply requested state update because the ticket only holds a
        /// read lock on the given key.
        state_update_with_read_lock,
        /// Cannot commit the ticket because the ticket has not been prepared.
        not_prepared,
        /// Request invalid because ticket is in the committed state.
        committed,
        /// Request invalid because ticket is not in the committed state.
        not_committed,
        /// Request failed because of a transient internal error.
        internal_error
    };

    /// Details about wounded error code
    struct wounded_details {
        /// The ticket that caused wounding
        ticket_number_type m_wounding_ticket{};

        /// The key that triggered the other ticket to wound
        key_type m_wounding_key;
    };

    // An error occurring
    struct shard_error {
        /// The error code
        error_code m_error_code{};

        /// Optional details about the wounded error code
        std::optional<wounded_details> m_wounded_details;
    };

    /// Ticket states returned by shards for broker recovery purposes.
    enum class ticket_state : uint8_t {
        /// Begun, may still hold locks or be rolled-back.
        begun,
        /// Wounded, not holding any locks.
        wounded,
        /// Prepared, holds locks.
        prepared,
        /// Committed, not holding any locks.
        committed
    };

    /// Interface for a runtime locking shard. Shard implements the three-phase
    /// commit protocol and two-phase locking. Deadlocks are avoided by
    /// assigning each transaction a monotonically increasing ticket number.
    /// Older tickets always receive higher priority than younger tickets. If
    /// an older ticket requests a lock on a key held by a younger ticket, the
    /// younger ticket is "wounded" (all its locks are revoked), and the lock
    /// is assigned to the older ticket. Once prepared, tickets are protected
    /// from being wounded until they are committed when their locks are
    /// released. Locks queue until they are asynchronously assigned to a
    /// ticket. The shard supports both read and write locks. Multiple readers
    /// are given the lock on a key at the same time, unless there is a write
    /// lock in the queue, in which case the write lock gets priority over new
    /// readers.
    class interface {
      public:
        virtual ~interface() = default;

        interface() = default;
        interface(const interface&) = delete;
        auto operator=(const interface&) -> interface& = delete;
        interface(interface&&) = delete;
        auto operator=(interface&&) -> interface& = delete;

        /// Return type from a try lock operation. Either the value at the
        /// requested key or an error code.
        using try_lock_return_type = std::variant<value_type, shard_error>;
        /// Function type for try lock operation results.
        using try_lock_callback_type
            = std::function<void(try_lock_return_type)>;

        /// Requests a lock on the given key and returns the value associated
        /// with the key. Lock may not be acquired immediately if another
        /// ticket already holds the write lock. May cause other tickets to be
        /// wounded if the requested lock is already held by a younger ticket.
        /// Cannot be used once a ticket is prepared or committed.
        /// \param ticket_number ticket number requesting the lock.
        /// \param broker_id broker ID managing the ticket.
        /// \param key key to lock.
        /// \param locktype type of lock to acquire.
        /// \param first_lock true if this is the first lock.
        /// \param result_callback function to call with the value or error
        ///                        code.
        /// \return true if the operation was initiated successfully.
        virtual auto try_lock(ticket_number_type ticket_number,
                              broker_id_type broker_id,
                              key_type key,
                              lock_type locktype,
                              bool first_lock,
                              try_lock_callback_type result_callback) -> bool
                                                                         = 0;

        /// Return type from a prepare operation. An error, if applicable.
        using prepare_return_type = std::optional<shard_error>;
        /// Callback function type for the result of a prepare operation.
        using prepare_callback_type = std::function<void(prepare_return_type)>;

        /// Prepares a ticket with the given state updates to be applied if the
        /// ticket is subsequently committed. Protects the ticket from being
        /// wounded by other tickets requesting locks.
        /// \param ticket_number ticket to prepare.
        /// \param broker_id broker ID managing the ticket.
        /// \param state_update state changes to apply if ticket is committed.
        /// \param result_callback function to call with prepare result.
        /// \return true if the operation was initiated successfully.
        virtual auto prepare(ticket_number_type ticket_number,
                             broker_id_type broker_id,
                             state_update_type state_update,
                             prepare_callback_type result_callback) -> bool
                                                                       = 0;

        /// Return type from a commit operation. An error code, if applicable.
        using commit_return_type = std::optional<shard_error>;
        /// Callback function type for the result of a commit operation.
        using commit_callback_type = std::function<void(commit_return_type)>;

        /// Commits the state updates from a previously prepared ticket. Writes
        /// the changes from the state update and unlocks any locks held by the
        /// ticket.
        /// \param ticket_number ticket to commit.
        /// \param result_callback function to call with the commit result.
        /// \return true if the operation was initiated successfully.
        virtual auto commit(ticket_number_type ticket_number,
                            commit_callback_type result_callback) -> bool = 0;

        /// Return type from a rollback operation. An error code, if
        /// applicable.
        using rollback_return_type = std::optional<shard_error>;
        /// Callback function type for the result of a rollback operation.
        using rollback_callback_type
            = std::function<void(rollback_return_type)>;

        /// Releases any locks held by a ticket and returns it to a clean
        /// state. Used to abort a wounded ticket, a ticket which experienced
        /// an irrecoverable error during execution, or cancel a prepared
        /// ticket. Cannot be used after a ticket is committed.
        /// \param ticket_number ticket to roll back.
        /// \param result_callback function to call with rollback result.
        /// \return true if the operation was initiated successfully.
        virtual auto rollback(ticket_number_type ticket_number,
                              rollback_callback_type result_callback) -> bool
                                                                         = 0;

        /// Return type from a finish operation. An error code, if applicable.
        using finish_return_type = std::optional<shard_error>;
        /// Callback function type for the result of a finish operation.
        using finish_callback_type = std::function<void(finish_return_type)>;

        /// Removes a ticket from the shard's internal state. Called after a
        /// commit or rollback operation to permanently complete a ticket. No
        /// further operations should be performed on the ticket after a
        /// successful finish operation.
        /// \param ticket_number ticket to finish.
        /// \param result_callback function to call with finish result.
        /// \return true if the operation was initiated successfully.
        virtual auto finish(ticket_number_type ticket_number,
                            finish_callback_type result_callback) -> bool = 0;

        /// Return type from a successful get tickets operation. A map of
        /// ticket numbers to their state.
        using get_tickets_success_type
            = std::unordered_map<ticket_number_type, ticket_state>;
        /// Return type from a get tickets operation. Either a map of ticket
        /// states or an error code.
        using get_tickets_return_type
            = std::variant<get_tickets_success_type, error_code>;
        /// Callback function type for the result of a get tickets operation.
        using get_tickets_callback_type
            = std::function<void(get_tickets_return_type)>;

        /// Returns all unfinished tickets managed with the given broker ID.
        /// \param broker_id broker ID.
        /// \param result_callback function to call with get tickets result.
        /// \return true if the operation was initiated successfully.
        virtual auto
        get_tickets(broker_id_type broker_id,
                    get_tickets_callback_type result_callback) -> bool = 0;
    };
}

#endif
