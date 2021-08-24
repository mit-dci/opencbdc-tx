// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CBDC_UNIVERSE0_SRC_3PC_UTIL_H_
#define CBDC_UNIVERSE0_SRC_3PC_UTIL_H_

#include "broker/interface.hpp"
#include "util/common/config.hpp"
#include "util/common/logging.hpp"

namespace cbdc::threepc {
    /// Type of load to generate for benchmarking
    enum class load_type {
        /// Base token transfer
        transfer,
        /// ERC20 token transfer
        erc20
    };

    /// Execution/transaction model
    enum class runner_type {
        /// Ethereum-style transactions using EVM.
        evm,
        /// Transaction semantics defined using Lua.
        lua
    };

    /// Configuration parameters for a phase two system.
    struct config {
        /// RPC endpoints for the nodes in the ticket machine raft cluster.
        std::vector<network::endpoint_t> m_ticket_machine_endpoints;
        /// RPC endpoints for the agents.
        std::vector<network::endpoint_t> m_agent_endpoints;
        /// RPC endpoints for the nodes in the shard raft clusters.
        std::vector<std::vector<network::endpoint_t>> m_shard_endpoints;
        /// ID of the component the instance should be.
        size_t m_component_id;
        /// ID of the node within the component the instance should be, if
        /// applicable.
        std::optional<size_t> m_node_id;
        /// Log level to use, defaults to WARN
        logging::log_level m_loglevel;
        /// The number of simultaneous load generator threads
        size_t m_loadgen_accounts;
        /// Type of transactions load generators should produce
        load_type m_load_type;
        /// Toggle for enabling telemetry
        bool m_enable_telemetry;
        /// The percentage of transactions that are using the same account
        /// to simulate contention
        double m_contention_rate;
        /// Type of execution environment to use in the agent.
        runner_type m_runner_type{runner_type::evm};
    };

    /// Reads the configuration parameters from the program arguments.
    /// \param argc number of program arguments.
    /// \param argv program arguments.
    /// \return configuration parametrs or std::nullopt if there was an error
    ///         while parsing the arguments.
    auto read_config(int argc, char** argv) -> std::optional<config>;

    /// Asynchronously inserts the given row into the cluster.
    /// \param broker broker to use for inserting the row.
    /// \param key key at which to insert value.
    /// \param value value to insert at given key.
    /// \param result_callback function to call on insertion success or
    ///                        failure.
    /// \return true if request was initiated successfully.
    auto put_row(const std::shared_ptr<broker::interface>& broker,
                 broker::key_type key,
                 broker::value_type value,
                 const std::function<void(bool)>& result_callback) -> bool;
}

#endif
