// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CBDC_UNIVERSE0_SRC_3PC_UTIL_H_
#define CBDC_UNIVERSE0_SRC_3PC_UTIL_H_

#include "util/common/config.hpp"
#include "util/common/logging.hpp"

namespace cbdc::threepc {
    /// Configuration parameters for a phase two system.
    struct config {
        /// RPC endpoints for the nodes in the ticket machine raft cluster.
        std::vector<network::endpoint_t> m_ticket_machine_endpoints;
        /// ID of the component the instance should be.
        size_t m_component_id;
        /// Log level to use, defaults to WARN
        logging::log_level m_loglevel;
    };

    /// Reads the configuration parameters from the program arguments.
    /// \param argc number of program arguments.
    /// \param argv program arguments.
    /// \return configuration parametrs or std::nullopt if there was an error
    ///         while parsing the arguments.
    auto read_config(int argc, char** argv) -> std::optional<config>;
}

#endif
