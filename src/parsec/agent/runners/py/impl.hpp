// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_AGENT_PY_RUNNER_H_
#define OPENCBDC_TX_SRC_PARSEC_AGENT_PY_RUNNER_H_

#include "parsec/agent/runners/interface.hpp"
#include "parsec/util.hpp"

#include <Python.h>
#include <future>
#include <memory>

namespace cbdc::parsec::agent::runner {
    /// Py function executor. Provides an environment for contracts to execute
    class py_runner : public interface {
      public:
        /// \copydoc interface::interface()
        py_runner(std::shared_ptr<logging::log> logger,
                  const cbdc::parsec::config& cfg,
                  runtime_locking_shard::value_type function,
                  parameter_type param,
                  bool is_readonly_run,
                  run_callback_type result_callback,
                  try_lock_callback_type try_lock_callback,
                  std::shared_ptr<secp256k1_context> secp,
                  std::shared_ptr<thread_pool> t_pool,
                  ticket_number_type ticket_number);

        /// Begins function execution. Retrieves the function bytecode using a
        /// read lock and executes it with the given parameter.
        /// \return true unless a internal system error has occurred
        [[nodiscard]] auto run() -> bool override;

        /// Lock type to acquire when requesting the function code.
        static constexpr auto initial_lock_type = broker::lock_type::read;

      private:
        // Store the input arguments for the function to be executed
        std::vector<std::string> m_input_args;

        // Store the names of the arguments to be pulled from the Python
        // environment
        std::vector<std::string> m_return_args;

        // Store the values associated with m_return args. These are used to
        // update state.
        std::vector<runtime_locking_shard::value_type> m_return_values;

        // Store the keys which are to be updated as a result of this
        // computation
        std::vector<runtime_locking_shard::key_type> m_update_keys;

        // The inputs to the function which are stored on the shards.
        // Therefore, the values are not necessarially known to the caller.
        std::vector<runtime_locking_shard::key_type> m_shard_inputs;

        // Tracks the expected types of values returned from this function.
        std::string m_return_types;

        // At the end of execution, pull the relevant values from the Python
        // state and send update(s) to the shards
        void update_state(PyObject* localDictionary);

        // Parse the function params into something usable by the Python VM
        auto parse_params() -> std::vector<std::string>;

        // Parse the function header
        // The function header is expected to contain information about the
        // function arguments and return values.
        // Fills m_input_args, m_return_args, m_return_types.
        // Trims the function header from m_function.
        void parse_header();

        // Pull the relevant values from the Python state
        // Helper for update_state()
        void get_state_updates(PyObject* localDictionary);

        // Handle the return of a try lock request. Relevant for communicating
        // information within the runner scope.
        /// \note As implemented, this method does not store the value returned
        /// from the shards on success
        void
        handle_try_lock(const broker::interface::try_lock_return_type& res);

        // Handle a try lock request but if a value is returned from the
        // shards, store it somewhere in the runner scope. Useful for getting
        // arguments from the shards to pass into the VM.
        /// \note This is where shard-dependent input arguments are collected from the shards
        auto handle_try_lock_input_arg(
            const broker::interface::try_lock_return_type& res,
            broker::value_type& dest) -> bool;
    };
}

#endif
