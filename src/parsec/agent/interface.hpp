// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_AGENT_INTERFACE_H_
#define OPENCBDC_TX_SRC_PARSEC_AGENT_INTERFACE_H_

#include "parsec/broker/interface.hpp"

namespace cbdc::parsec::agent {
    /// Type of function call parameter.
    using parameter_type = buffer;
    /// Type returned after function execution.
    using return_type = broker::state_update_type;

    /// Interface for an agent. Manages the lifetime of a single transaction/
    /// function execution/ticket and communication with the broker.
    class interface {
      public:
        virtual ~interface() = default;

        interface(const interface&) = delete;
        auto operator=(const interface&) -> interface& = delete;
        interface(interface&&) = delete;
        auto operator=(interface&&) -> interface& = delete;

        /// Error codes returned by agent operations.
        enum class error_code : uint8_t {
            /// Broker was unreachable.
            broker_unreachable,
            /// Ticket number assignment failed.
            ticket_number_assignment,
            /// Error retrieving function bytecode.
            function_retrieval,
            /// Error during function execution.
            function_execution,
            /// Error committing the function state updates.
            commit_error,
            /// Error finishing the ticket.
            finish_error,
            /// Error during rollback.
            rollback_error,
            /// Transient error, execution should be retried.
            retry
        };

        /// Return type from function execution. Either the committed state
        /// updates or an error code.
        using exec_return_type = std::variant<return_type, error_code>;
        /// Callback function type with function execution result.
        using exec_callback_type = std::function<void(exec_return_type)>;

        /// Constructor.
        /// \param function key where function bytecode is located.
        /// \param param parameter to call function with.
        /// \param result_callback function to call with execution result.
        interface(runtime_locking_shard::key_type function,
                  parameter_type param,
                  exec_callback_type result_callback);

        /// Executes the function managed by this agent with the given
        /// parameter.
        /// \return true if function execution was intiated successfully.
        virtual auto exec() -> bool = 0;

        /// Return the key of the function bytecode managed by this agent.
        /// \return function bytecode key.
        [[nodiscard]] auto
        get_function() const -> runtime_locking_shard::key_type;

        /// Return the function parameter managed by this agent.
        /// \return function parameter.
        [[nodiscard]] auto get_param() const -> parameter_type;

        /// Return the result callback function stored by this agent.
        /// \return result callback function.
        [[nodiscard]] auto get_result_callback() const -> exec_callback_type;

      private:
        runtime_locking_shard::key_type m_function;
        parameter_type m_param;
        exec_callback_type m_result_callback;
    };
}

#endif
