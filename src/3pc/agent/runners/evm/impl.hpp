// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_AGENT_EVM_RUNNER_H_
#define OPENCBDC_TX_SRC_3PC_AGENT_EVM_RUNNER_H_

#include "3pc/agent/runners/interface.hpp"
#include "3pc/util.hpp"
#include "host.hpp"

#include <evmc/evmc.h>
#include <secp256k1.h>
#include <thread>

namespace cbdc::threepc::agent::runner {
    /// Commands accepted by the EVM contract runner.
    enum class evm_runner_function : uint8_t {
        /// Execute a normal transaction.
        execute_transaction,
        /// Read the metadata of an account.
        read_account,
        /// Execute a transaction without applying any changes.
        dryrun_transaction,
        /// Read the contract code of an account.
        read_account_code,
        /// Return a previously completed transaction.
        get_transaction,
        /// Return the receipt for a transaction.
        get_transaction_receipt,
        /// Return just the ticket number to simulate getting the
        /// latest block
        get_block_number,
        /// Return a pretend block that is based on the ticket
        /// number, and the transaction (potentially) corresponding
        /// to it
        get_block,
        /// Query the logs for a particular address, block range and
        /// topic filter
        get_logs,
        /// Read a specific key of an account's storage
        read_account_storage,
    };

    /// Executes EVM transactions, implementing the runner interface.
    class evm_runner : public interface {
      public:
        /// \copydoc interface::interface
        evm_runner(std::shared_ptr<logging::log> logger,
                   const cbdc::threepc::config& cfg,
                   runtime_locking_shard::value_type function,
                   parameter_type param,
                   bool is_readonly_run,
                   run_callback_type result_callback,
                   try_lock_callback_type try_lock_callback,
                   std::shared_ptr<secp256k1_context> secp,
                   std::shared_ptr<thread_pool> t_pool,
                   ticket_number_type ticket_number);

        /// Blocks until the transaction has completed and all processing
        /// threads have ended.
        ~evm_runner() override;

        evm_runner(const evm_runner&) = delete;
        auto operator=(const evm_runner&) -> evm_runner& = delete;
        evm_runner(evm_runner&&) = delete;
        auto operator=(evm_runner&&) -> evm_runner& = delete;

        /// Begin executing the transaction asynchronously.
        /// \return true if execution was initiated successfully.
        auto run() -> bool override;

        /// Initial lock type for the agent to request when retrieving the
        /// function key.
        static constexpr auto initial_lock_type = broker::lock_type::write;

      private:
        std::vector<std::thread> m_evm_threads;

        std::unique_ptr<evm_host> m_host;
        evm_tx m_tx;
        evmc_message m_msg{};

        void exec();
        auto run_execute_real_transaction() -> bool;
        auto run_execute_dryrun_transaction() -> bool;
        auto run_get_account_code() -> bool;
        auto run_get_transaction() -> bool;
        auto run_get_transaction_receipt() -> bool;
        auto run_execute_transaction(const evmc::address& from,
                                     bool is_readonly_run) -> bool;
        auto run_get_account() -> bool;
        auto run_get_block() -> bool;
        auto run_get_logs() -> bool;
        auto run_get_block_number() -> bool;
        [[nodiscard]] static auto check_base_gas(const evm_tx& tx,
                                                 bool is_readonly_run)
            -> std::pair<evmc::uint256be, bool>;
        [[nodiscard]] static auto make_tx_context(const evmc::address& from,
                                                  const evm_tx& tx,
                                                  bool is_readonly_run)
            -> evmc_tx_context;
        static auto make_message(const evmc::address& from,
                                 const evm_tx& tx,
                                 bool is_readonly_run)
            -> std::pair<evmc_message, bool>;

        void handle_lock_from_account(
            const broker::interface::try_lock_return_type& res);

        void lock_ticket_number_key();
        void lock_index_keys(const std::function<void()>& callback);
        void schedule_exec();

        void schedule(const std::function<void()>& fn);

        void schedule_run();
        void do_run();
        static auto make_pretend_block(interface::ticket_number_type tn)
            -> evm_pretend_block;

        void handle_get_logs_try_lock_response(
            const evm_log_query& qry,
            const std::shared_ptr<std::vector<evm_log_index>>& log_indexes,
            const std::shared_ptr<std::atomic<size_t>>& acquired,
            const std::shared_ptr<std::mutex>& log_indexes_mut,
            size_t key_count,
            const broker::interface::try_lock_return_type& res);

        void handle_complete_get_logs(
            const evm_log_query& qry,
            const std::shared_ptr<std::mutex>& log_indexes_mut,
            const std::shared_ptr<std::vector<evm_log_index>>& log_indexes);

        void lock_tx_receipt(const broker::value_type& value,
                             const ticket_number_type& ticket_number);
    };
}

#endif
