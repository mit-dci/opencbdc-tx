// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_TICKET_MACHINE_MESSAGES_H_
#define OPENCBDC_TX_SRC_3PC_TICKET_MACHINE_MESSAGES_H_

#include "interface.hpp"

namespace cbdc::threepc::ticket_machine::rpc {
    /// Ticket machine RPC request type.
    using request = std::variant<std::monostate>;
    /// Ticket machine RPC response type.
    using response = interface::get_ticket_number_return_type;
}

#endif
