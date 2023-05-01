// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_AGENT_RUNNER_H_
#define OPENCBDC_TX_SRC_3PC_AGENT_RUNNER_H_

#include "3pc/agent/runners/interface.hpp"
#include "3pc/util.hpp"

#include <lua.hpp>
#include <memory>

namespace cbdc::threepc::agent::runner {
    /// Lua function executor. Provides an environment for contracts to execute
    /// in. Manages retrieval of function bytecode, locking keys during
    /// function execution, signature checking and commiting execution results.
    /// Class cannot be re-used for different functions/transactions, manages
    /// the lifecycle of a single transaction.
    class lua_runner : public interface {
      public:
        /// \copydoc interface::interface()
        lua_runner(std::shared_ptr<logging::log> logger,
                   const cbdc::threepc::config& cfg,
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
        /// \return true.
        auto run() -> bool override;

        /// Lock type to acquire when requesting the function code.
        static constexpr auto initial_lock_type = broker::lock_type::read;

      private:
        std::shared_ptr<lua_State> m_state;

        void contract_epilogue(int n_results);

        auto get_stack_string(int index) -> std::optional<buffer>;

        void schedule_contract();

        void
        handle_try_lock(const broker::interface::try_lock_return_type& res);

        static auto check_sig(lua_State* L) -> int;
    };
}

#endif
