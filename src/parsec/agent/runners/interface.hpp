// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_AGENT_RUNNERS_INTERFACE_H_
#define OPENCBDC_TX_SRC_PARSEC_AGENT_RUNNERS_INTERFACE_H_

#include "parsec/agent/interface.hpp"
#include "parsec/broker/interface.hpp"
#include "parsec/runtime_locking_shard/interface.hpp"
#include "parsec/util.hpp"
#include "util/common/logging.hpp"
#include "util/common/thread_pool.hpp"

#include <memory>

namespace cbdc::parsec::agent::runner {
    /// Interface for a contract runner. Subclasses should implement
    /// application logic to enforce specific transaction semantics.
    class interface {
      public:
        /// Error codes return during function execution.
        enum class error_code {
            /// Function did not return a string value.
            result_value_type,
            /// Function did not return a string key.
            result_key_type,
            /// Function did not return a map.
            result_type,
            /// Function more than one result.
            result_count,
            /// Runner error during function execution.
            exec_error,
            /// Error loading function bytecode.
            function_load,
            /// Internal Runner error.
            internal_error,
            /// Function yielded more than one key to lock.
            yield_count,
            /// Function yielded a invalid datatype.
            yield_type,
            /// Error acquiring lock on key.
            lock_error,
            /// Ticket wounded during execution.
            wounded
        };

        /// Type alias for a ticket number.
        using ticket_number_type = parsec::ticket_machine::ticket_number_type;

        /// Return type from executing a function. Either the state updates
        /// committed after function execution or an error code.
        using run_return_type
            = std::variant<runtime_locking_shard::state_update_type,
                           error_code>;
        /// Callback type for function execution.
        using run_callback_type = std::function<void(run_return_type)>;

        /// Callback function type for acquiring locks during function
        /// execution. Accepts a key to lock and function to call with lock
        /// result. Returns true if request was initiated successfully.
        using try_lock_callback_type
            = std::function<bool(broker::key_type,
                                 broker::lock_type,
                                 broker::interface::try_lock_callback_type)>;

        /// Factory function type for instantiating new runners.
        using factory_type = std::function<std::unique_ptr<interface>(
            std::shared_ptr<logging::log> logger,
            const cbdc::parsec::config& cfg,
            runtime_locking_shard::value_type function,
            parameter_type param,
            bool is_readonly_run,
            runner::interface::run_callback_type result_callback,
            runner::interface::try_lock_callback_type try_lock_callback,
            std::shared_ptr<secp256k1_context>,
            std::shared_ptr<thread_pool> t_pool,
            ticket_number_type ticket_number)>;

        /// Constructor.
        /// \param logger log instance.
        /// \param cfg config reference.
        /// \param function key of function bytecode to execute.
        /// \param param parameter to pass to function.
        /// \param is_readonly_run true if runner execution should not result in state
        ///                changes.
        /// \param result_callback function to call with function execution
        ///                        result.
        /// \param try_lock_callback function to call for the function to
        ///                          request key locks.
        /// \param secp shared context for libsecp256k1.
        /// \param t_pool shared thread pool between agents.
        /// \param ticket_number ticket number for the ticket managed by this
        ///                      runner instance.
        interface(std::shared_ptr<logging::log> logger,
                  const cbdc::parsec::config& cfg,
                  runtime_locking_shard::value_type function,
                  parameter_type param,
                  bool is_readonly_run,
                  run_callback_type result_callback,
                  try_lock_callback_type try_lock_callback,
                  std::shared_ptr<secp256k1_context> secp,
                  std::shared_ptr<thread_pool> t_pool,
                  ticket_number_type ticket_number);

        virtual ~interface() = default;

        interface(const interface&) = delete;
        auto operator=(const interface&) -> interface& = delete;
        interface(interface&&) = delete;
        auto operator=(interface&&) -> interface& = delete;

        /// Begins function execution. Retrieves the function bytecode using a
        /// read lock and executes it with the given parameter.
        /// \return true unless a internal system error has occurred
        [[nodiscard]] virtual auto run() -> bool = 0;

        friend class lua_runner;
        friend class evm_runner;

      private:
        std::shared_ptr<logging::log> m_log;
        const cbdc::parsec::config& m_cfg;
        runtime_locking_shard::value_type m_function;
        parameter_type m_param;
        bool m_is_readonly_run;
        run_callback_type m_result_callback;
        try_lock_callback_type m_try_lock_callback;
        std::shared_ptr<secp256k1_context> m_secp;
        std::shared_ptr<thread_pool> m_threads;
        ticket_number_type m_ticket_number;
    };

    /// Runner factory for agents to intiantiate new runners of a particular
    /// type while only worrying about the runner interface.
    /// \tparam T runner implementation to construct.
    template<class T>
    class factory {
      public:
        /// Construct a new runner of type T.
        /// \return new runner.
        static auto
        create(std::shared_ptr<logging::log> logger,
               cbdc::parsec::config cfg,
               runtime_locking_shard::value_type function,
               parameter_type param,
               bool is_readonly_run,
               runner::interface::run_callback_type result_callback,
               runner::interface::try_lock_callback_type try_lock_callback,
               std::shared_ptr<secp256k1_context> secp,
               std::shared_ptr<thread_pool> t_pool,
               runner::interface::ticket_number_type ticket_number)
            -> std::unique_ptr<runner::interface> {
            return std::make_unique<T>(std::move(logger),
                                       std::move(cfg),
                                       std::move(function),
                                       std::move(param),
                                       is_readonly_run,
                                       std::move(result_callback),
                                       std::move(try_lock_callback),
                                       std::move(secp),
                                       std::move(t_pool),
                                       ticket_number);
        }
    };
}

#endif
