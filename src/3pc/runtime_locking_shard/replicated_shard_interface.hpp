// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CBDC_UNIVERSE0_SRC_3PC_RUNTIME_LOCKING_SHARD_REPLICATED_SHARD_INTERFACE_H_
#define CBDC_UNIVERSE0_SRC_3PC_RUNTIME_LOCKING_SHARD_REPLICATED_SHARD_INTERFACE_H_

#include "interface.hpp"

namespace cbdc::threepc::runtime_locking_shard {
    /// Interface for replicating internal state for prepared and committed
    /// tickets managed by a locking shard.
    class replicated_shard_interface {
      public:
        /// Error codes returned by class methods.
        enum class error_code : uint8_t {
            /// Requested ticket does not exist.
            unknown_ticket,
            /// Internal error preventing processing.
            internal_error
        };

        /// Ticket states returned by shards for broker recovery purposes.
        enum class ticket_state : uint8_t {
            /// Prepared, holds locks.
            prepared,
            /// Committed, not holding any locks.
            committed
        };

        /// Ticket date stored in the replicated state machine.
        struct ticket_type {
            /// Broker managing the ticket.
            broker_id_type m_broker_id{};
            /// State updated to apply after commit.
            state_update_type m_state_update;
            /// State of the ticket within the 3PC protocol.
            ticket_state m_state{};
        };

        /// Type for state updates to a shard. A map of keys and their new
        /// values.
        using state_type
            = std::unordered_map<key_type,
                                 value_type,
                                 hashing::const_sip_hash<key_type>>;

        /// Type for the tickets list returned by the state machine.
        using tickets_type
            = std::unordered_map<ticket_number_type, ticket_type>;

        /// Return type from a prepare operation. An error, if applicable.
        using return_type = std::optional<error_code>;
        /// Callback function type for the result of a prepare operation.
        using callback_type = std::function<void(return_type)>;

        virtual ~replicated_shard_interface() = default;

        replicated_shard_interface() = default;
        replicated_shard_interface(const replicated_shard_interface&)
            = default;
        auto operator=(const replicated_shard_interface&)
            -> replicated_shard_interface& = default;
        replicated_shard_interface(replicated_shard_interface&&) = default;
        auto operator=(replicated_shard_interface&&)
            -> replicated_shard_interface& = default;

        /// Stores a prepare request for a ticket in the state machine.
        /// \param ticket_number ticket to prepare.
        /// \param broker_id broker managing the ticket.
        /// \param state_update keys and values to update after commit.
        /// \param result_callback function to call with prepare result.
        /// \return true if operation was initiated successfully.
        virtual auto prepare(ticket_number_type ticket_number,
                             broker_id_type broker_id,
                             state_type state_update,
                             callback_type result_callback) -> bool
            = 0;

        /// Stores a commit request in the state machine.
        /// \param ticket_number ticket to commit.
        /// \param result_callback function to call with commit result.
        /// \return true if operation was initiated successfully.
        virtual auto commit(ticket_number_type ticket_number,
                            callback_type result_callback) -> bool
            = 0;

        /// Stores a finish request in the state machine.
        /// \param ticket_number ticket to finish.
        /// \param result_callback function to call with finish result.
        /// \return true if operation was initiated successfully.
        virtual auto finish(ticket_number_type ticket_number,
                            callback_type result_callback) -> bool
            = 0;

        /// Return type from a get tickets operation. Either a map of ticket
        /// states or an error code.
        using get_tickets_return_type = std::variant<tickets_type, error_code>;
        /// Callback function type for the result of a get tickets operation.
        using get_tickets_callback_type
            = std::function<void(get_tickets_return_type)>;

        /// Retrieves unfinished tickets from the state machine.
        /// \param result_callback function to call with the tickets held by
        ///                        the state machine.
        /// \return true if operation was initiated successfully.
        [[nodiscard]] virtual auto
        get_tickets(get_tickets_callback_type result_callback) const -> bool
            = 0;
    };
}

#endif
