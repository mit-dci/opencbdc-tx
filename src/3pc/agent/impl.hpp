// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_AGENT_IMPL_H_
#define OPENCBDC_TX_SRC_3PC_AGENT_IMPL_H_

#include "3pc/agent/runners/interface.hpp"
#include "3pc/broker/interface.hpp"
#include "interface.hpp"
#include "util/common/logging.hpp"

namespace cbdc::threepc::agent {
    /// Implementation of an agent.
    class impl : public interface {
      public:
        /// States for a ticket managed by this agent.
        enum class state {
            /// Initial state.
            init,
            /// Begin request sent to broker.
            begin_sent,
            /// Begin request failed.
            begin_failed,
            /// Function bytecode lock sent to broker.
            function_get_sent,
            /// Function bytecode lock request failed.
            function_get_failed,
            /// Broker error during function bytecode lock.
            function_get_error,
            /// Function execution started.
            function_started,
            /// Function execution failed.
            function_failed,
            /// Function error during execution.
            function_error,
            /// Commit request sent to broker.
            commit_sent,
            /// Commit request failed.
            commit_failed,
            /// Broker error during commit request.
            commit_error,
            /// Finish request sent to broker.
            finish_sent,
            /// Finish request failed.
            finish_failed,
            /// Finish complete.
            finish_complete,
            /// Rollback request sent to broker.
            rollback_sent,
            /// Rollback request failed.
            rollback_failed,
            /// Rollback complete.
            rollback_complete
        };

        /// Constructor.
        /// \param logger log instance.
        /// \param cfg config instance.
        /// \param runner_factory function which constructs and returns a
        ///                       pointer to a runner instance.
        /// \param broker broker instance.
        /// \param function key containing function bytecode.
        /// \param param function parameter.
        /// \param result_callback function to call with function execution
        ///                        result.
        /// \param initial_lock_type type of lock to acquire on initial
        ///                          function code.
        /// \param is_readonly_run true if the agent should skip writing state changes.
        /// \param secp secp256k1 context.
        /// \param t_pool shared thread pool between all agents.
        impl(std::shared_ptr<logging::log> logger,
             cbdc::threepc::config cfg,
             runner::interface::factory_type runner_factory,
             std::shared_ptr<broker::interface> broker,
             runtime_locking_shard::key_type function,
             parameter_type param,
             exec_callback_type result_callback,
             broker::lock_type initial_lock_type,
             bool is_readonly_run,
             std::shared_ptr<secp256k1_context> secp,
             std::shared_ptr<thread_pool> t_pool);

        /// Ensures function execution is complete before destruction.
        ~impl() override;

        impl(const impl&) = delete;
        auto operator=(const impl&) -> impl& = delete;
        impl(impl&&) = delete;
        auto operator=(impl&&) -> impl& = delete;

        /// Initiates function execution.
        /// \return true.
        auto exec() -> bool override;

        /// Returns the ticket number associated with this agent, if available.
        /// \return ticket number, or std::nullopt if no ticket number has been
        ///         assigned yet.
        auto get_ticket_number() const
            -> std::optional<ticket_machine::ticket_number_type>;

        /// Return the state of the ticket.
        /// \return ticket state.
        auto get_state() const -> state;

      private:
        std::shared_ptr<logging::log> m_log;
        const cbdc::threepc::config m_cfg;
        runner::interface::factory_type m_runner_factory;
        std::shared_ptr<broker::interface> m_broker;
        std::optional<ticket_machine::ticket_number_type> m_ticket_number;
        std::optional<exec_return_type> m_result;
        std::unique_ptr<runner::interface> m_runner;
        state m_state{state::init};
        bool m_permanent_error{false};
        mutable std::recursive_mutex m_mut;
        broker::lock_type m_initial_lock_type;
        bool m_is_readonly_run;
        std::shared_ptr<secp256k1_context> m_secp;
        std::shared_ptr<thread_pool> m_threads;
        std::optional<hash_t> m_tx_id;
        bool m_wounded{false};
        broker::held_locks_set_type m_requested_locks{};
        bool m_restarted{false};

        void handle_begin(broker::interface::ticketnum_or_errcode_type res);

        void
        handle_function(const broker::interface::try_lock_return_type& res);

        void handle_run(const runner::interface::run_return_type& res);

        void handle_commit(broker::interface::commit_return_type res);

        void do_start();

        void do_result();

        void do_finish();

        void do_runner(broker::value_type v);

        void do_rollback(bool finish);

        void do_commit();

        [[nodiscard]] auto
        do_try_lock_request(broker::key_type key,
                            broker::lock_type locktype,
                            broker::interface::try_lock_callback_type res_cb)
            -> bool;

        void
        handle_rollback(broker::interface::rollback_return_type rollback_res);

        void handle_finish(broker::interface::finish_return_type finish_res);

        void handle_try_lock_response(
            const broker::interface::try_lock_callback_type& res_cb,
            broker::interface::try_lock_return_type res);
    };
}

#endif
