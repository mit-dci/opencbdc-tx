// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_AGENT_MESSAGES_H_
#define OPENCBDC_TX_SRC_PARSEC_AGENT_MESSAGES_H_

#include "interface.hpp"

namespace cbdc::parsec::agent::rpc {
    /// Agent contract execution RPC request message.
    struct exec_request {
        /// Key of function bytecode.
        runtime_locking_shard::key_type m_function;
        /// Function call parameter.
        parameter_type m_param;
        /// Whether the request should skip writing state changes.
        bool m_is_readonly_run{false};
    };

    /// Agent RPC request type.
    using request = exec_request;
    /// Agent RPC response type.
    using response = interface::exec_return_type;
}

#endif
