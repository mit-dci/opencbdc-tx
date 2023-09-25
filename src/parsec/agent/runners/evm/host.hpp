// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_AGENT_EVM_HOST_H_
#define OPENCBDC_TX_SRC_PARSEC_AGENT_EVM_HOST_H_

#include "parsec/agent/runners/evm/messages.hpp"
#include "parsec/agent/runners/interface.hpp"
#include "util/serialization/util.hpp"

#include <evmc/evmc.hpp>
#include <map>
#include <set>

namespace cbdc::parsec::agent::runner {
    /// Implementation of the evmc::Host interface using PARSEC as the backend
    /// database. Manages the cached state during contract execution to support
    /// committing the final state updates or reverting while still charging
    /// gas.
    /// Undocumented functions below are inhereted from evmc::Host and are
    /// documented upstream.
    class evm_host : public evmc::Host {
      public:
        /// Constructs a new host instance.
        /// \param log log instance.
        /// \param try_lock_callback function for requesting locks on keys.
        /// \param tx_context evmc context in which the transaction will
        ///                   execute.
        /// \param tx transaction to execute.
        /// \param is_readonly_run true if no state changes should be applied.
        /// \param ticket_number ticket number for transaction.
        evm_host(std::shared_ptr<logging::log> log,
                 interface::try_lock_callback_type try_lock_callback,
                 evmc_tx_context tx_context,
                 evm_tx tx,
                 bool is_readonly_run,
                 interface::ticket_number_type ticket_number);

        [[nodiscard]] auto
        account_exists(const evmc::address& addr) const noexcept
            -> bool override;

        [[nodiscard]] auto get_storage(const evmc::address& addr,
                                       const evmc::bytes32& key) const noexcept
            -> evmc::bytes32 override final;

        auto set_storage(const evmc::address& addr,
                         const evmc::bytes32& key,
                         const evmc::bytes32& value) noexcept
            -> evmc_storage_status override final;

        [[nodiscard]] auto
        get_balance(const evmc::address& addr) const noexcept
            -> evmc::uint256be override final;

        [[nodiscard]] auto
        get_code_size(const evmc::address& addr) const noexcept
            -> size_t override final;

        [[nodiscard]] auto
        get_code_hash(const evmc::address& addr) const noexcept
            -> evmc::bytes32 override final;

        auto copy_code(const evmc::address& addr,
                       size_t code_offset,
                       uint8_t* buffer_data,
                       size_t buffer_size) const noexcept
            -> size_t override final;

        void
        selfdestruct(const evmc::address& addr,
                     const evmc::address& beneficiary) noexcept override final;

        auto call(const evmc_message& msg) noexcept
            -> evmc::result override final;

        [[nodiscard]] auto get_tx_context() const noexcept
            -> evmc_tx_context override final;

        [[nodiscard]] auto get_block_hash(int64_t number) const noexcept
            -> evmc::bytes32 override final;

        void emit_log(
            const evmc::address& addr,
            const uint8_t* data,
            size_t data_size,
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            const evmc::bytes32 topics[],
            size_t topics_count) noexcept override final;

        auto access_account(const evmc::address& addr) noexcept
            -> evmc_access_status override final;

        auto access_storage(const evmc::address& addr,
                            const evmc::bytes32& key) noexcept
            -> evmc_access_status override final;

        using indexed_logs_type
            = std::unordered_map<evmc::address, std::vector<evm_log>>;

        /// Return the keys of the log indexes - these are sha256(addr, ticket)
        /// and will get value 1 - to indicate there are logs for the given
        /// address in the given ticket. The logs for the specific ticket can
        /// then be fetched and filtered on topic and address.
        /// \return list of keys to set to 1 for the log index
        auto get_log_index_keys() const -> std::vector<cbdc::buffer>;

        /// Return the changes to the state resulting from transaction
        /// execution.
        /// \return list of updates keys and values.
        auto get_state_updates() const
            -> runtime_locking_shard::state_update_type;

        /// Returns whether the transaction needs to be retried due to a
        /// transient error.
        /// \return true if the transaction needs to be re-executed.
        auto should_retry() const -> bool;

        /// Inserts an account into the host. The host will assume the lock is
        /// already held on the account metadata.
        /// \param addr account address.
        /// \param acc account metadata.
        void insert_account(const evmc::address& addr, const evm_account& acc);

        /// Finalizes the state updates resulting from the transaction.
        /// \param gas_left remaining unspent gas.
        /// \param gas_used total gas consumed by the transaction.
        void finalize(int64_t gas_left, int64_t gas_used);

        /// Set the state updates to revert the transaction changes due to a
        /// contract error.
        void revert();

        /// Return the key for the host's ticket number, which is the hash
        /// of the ticket number's serialized representation to uniformly
        /// distribute all ticket-to-tx mappings across the shards
        /// @return ticket number key
        auto ticket_number_key(std::optional<interface::ticket_number_type> tn
                               = std::nullopt) const -> cbdc::buffer;

        /// Return the key for the indicator of the existence of logs for
        /// a particular address at a particular ticket
        auto log_index_key(evmc::address addr,
                           std::optional<interface::ticket_number_type> tn
                           = std::nullopt) const -> cbdc::buffer;

      private:
        std::shared_ptr<logging::log> m_log;
        runner::interface::try_lock_callback_type m_try_lock_callback;
        mutable std::map<evmc::address,
                         std::pair<std::optional<evm_account>, bool>>
            m_accounts;
        mutable std::map<
            evmc::address,
            std::map<evmc::bytes32,
                     std::pair<std::optional<evmc::bytes32>, bool>>>
            m_account_storage;
        mutable std::map<evmc::address,
                         std::pair<std::optional<evm_account_code>, bool>>
            m_account_code;
        evmc_tx_context m_tx_context;
        std::unique_ptr<evmc::VM> m_vm;
        evm_tx m_tx;
        bool m_is_readonly_run;

        mutable std::set<evmc::address> m_accessed_addresses;
        std::set<std::pair<evmc::address, evmc::bytes32>>
            m_accessed_storage_keys;

        mutable bool m_retry{false};

        std::map<evmc::address, std::pair<std::optional<evm_account>, bool>>
            m_init_state;

        evm_tx_receipt m_receipt;
        cbdc::buffer m_tx_id;

        interface::ticket_number_type m_ticket_number;

        [[nodiscard]] auto get_account(const evmc::address& addr,
                                       bool write) const
            -> std::optional<evm_account>;

        [[nodiscard]] auto get_account_storage(const evmc::address& addr,
                                               const evmc::bytes32& key,
                                               bool write) const
            -> std::optional<evmc::bytes32>;

        [[nodiscard]] auto get_account_code(const evmc::address& addr,
                                            bool write) const
            -> std::optional<evm_account_code>;

        auto get_sorted_logs() const
            -> std::unordered_map<evmc::address, std::vector<evm_log>>;

        void transfer(const evmc::address& from,
                      const evmc::address& to,
                      const evmc::uint256be& value);

        static auto is_precompile(const evmc::address& addr) -> bool;

        [[nodiscard]] auto get_key(const cbdc::buffer& key, bool write) const
            -> std::optional<broker::value_type>;

        auto create(const evmc_message& msg) noexcept -> evmc::result;

        auto execute(const evmc_message& msg,
                     const uint8_t* code,
                     size_t code_size) -> evmc::result;
    };
}

#endif
