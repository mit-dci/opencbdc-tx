// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CBDC_UNIVERSE0_SRC_3PC_AGENT_RUNNERS_EVM_MESSAGES_H_
#define CBDC_UNIVERSE0_SRC_3PC_AGENT_RUNNERS_EVM_MESSAGES_H_

#include "3pc/agent/runners/interface.hpp"
#include "util/common/hash.hpp"

#include <evmc/evmc.hpp>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace cbdc::threepc::agent::runner {
    // EVM Chain ID for OpenCBDC
    static constexpr uint64_t opencbdc_chain_id = 0xcbdc;

    /// EVM account type
    struct evm_account {
        /// Balance in the account.
        evmc::uint256be m_balance{};
        /// Signature nonce.
        evmc::uint256be m_nonce{};

        /// Set of keys modified during contract execution.
        std::set<evmc::bytes32> m_modified{};
        /// Flag set if the account is being destructed.
        bool m_destruct{false};
    };

    /// Type alias for EVM account code.
    using evm_account_code = std::vector<uint8_t>;

    /// EVM signature type.
    struct evm_sig {
        evmc::uint256be m_r;
        evmc::uint256be m_s;
        evmc::uint256be m_v;
    };

    /// Type for tracking storage key accesses between accounts.
    struct evm_access_tuple {
        evmc::address m_address{};
        std::vector<evmc::bytes32> m_storage_keys{};
        auto operator==(const evm_access_tuple& rhs) const -> bool {
            return m_address == rhs.m_address
                && m_storage_keys == rhs.m_storage_keys;
        };
    };

    /// Type alias for a list of storage key accesses.
    using evm_access_list = std::vector<evm_access_tuple>;

    ///  EVM transaction types.
    enum class evm_tx_type : uint8_t {
        legacy = 0,
        access_list = 1,
        dynamic_fee = 2
    };

    /// EVM transaction type.
    struct evm_tx {
        /// Type of transaction.
        evm_tx_type m_type{};
        /// To address or std::nullopt if contract creation.
        std::optional<evmc::address> m_to{};
        /// Value to transfer.
        evmc::uint256be m_value{};
        /// Nonce for from account.
        evmc::uint256be m_nonce{};
        /// Gas price.
        evmc::uint256be m_gas_price{};
        /// Maximum gas for this transaction.
        evmc::uint256be m_gas_limit{};
        /// Maximum tip fee.
        evmc::uint256be m_gas_tip_cap{};
        /// Maximum base fee.
        evmc::uint256be m_gas_fee_cap{};
        /// Contract input data.
        std::vector<uint8_t> m_input{};
        /// List of storage key accesses.
        evm_access_list m_access_list{};
        /// Transaction signature.
        evm_sig m_sig;
    };

    /// Dry-run EVM transaction type.
    struct evm_dryrun_tx {
        /// From address.
        evmc::address m_from;
        /// EVM transaction to dry-run.
        evm_tx m_tx;
    };

    /// EVM log output type.
    struct evm_log {
        /// Address for the log.
        evmc::address m_addr{};
        /// Log data.
        std::vector<uint8_t> m_data{};
        /// List of log topics.
        std::vector<evmc::bytes32> m_topics{};
    };

    /// EVM transaction receipt type.
    struct evm_tx_receipt {
        /// EVM transaction.
        evm_tx m_tx;
        /// Created contract address, if applicable.
        std::optional<evmc::address> m_create_address;
        /// Gas used in transaction.
        evmc::uint256be m_gas_used{};
        /// List of logs emitted during transaction.
        std::vector<evm_log> m_logs{};
        /// EVM output data.
        std::vector<uint8_t> m_output_data{};
        /// Ticket number that ran this TX - needed to map
        /// to pretend blocks
        cbdc::threepc::agent::runner::interface::ticket_number_type
            m_ticket_number {};
        /// Timestamp of the transaction - needed to provide
        /// a timestamp in pretend blocks
        uint64_t m_timestamp{};
        // Success flag.
        bool m_success{false};
    };

    /// EVM pretend block is a pairing of the blocknumber (equal to the ticket
    /// number) and the transactions (currently always a single one) "inside
    /// the block" (executed by that ticket)
    struct evm_pretend_block {
        /// Ticket number
        interface::ticket_number_type m_ticket_number {};
        /// Transactions executed by the ticket
        std::vector<evm_tx_receipt> m_transactions{};
    };

    /// Describes the parameters of a query on EVM logs - used to transfer
    /// these parameters from the getLogs API method to the runner
    struct evm_log_query {
        /// The addresses for which logs are queried
        std::vector<evmc::address> m_addresses{};
        /// The topics for which logs are queried
        std::vector<evmc::bytes32> m_topics{};
        /// The start of the block range to query logs for
        cbdc::threepc::agent::runner::interface::ticket_number_type
            m_from_block {};
        /// The end of the block range to query logs for
        cbdc::threepc::agent::runner::interface::ticket_number_type
            m_to_block {};
    };

    /// Index data for evm logs. This is the value stored under a key
    /// calculated from the ticket number and the address, and it packs all
    /// of the logs for that address. We store a copy of the logs here, which
    /// is less efficient for storage, but prevents retrieving all logs for
    /// a transaction through its receipt, discarding the logs that are not
    /// related to a particular address
    struct evm_log_index {
        /// Ticket number that emitted the logs
        interface::ticket_number_type m_ticket_number {};
        /// TXID that emitted the logs
        cbdc::hash_t m_txid{};
        /// The logs that were emitted
        std::vector<evm_log> m_logs;
    };

    // Type for account code keys.
    struct code_key {
        /// Address for the account code.
        evmc::address m_addr;
    };

    /// Type for account storage keys.
    struct storage_key {
        /// Account address.
        evmc::address m_addr;
        /// Storage key.
        evmc::bytes32 m_key;
    };
}

#endif
