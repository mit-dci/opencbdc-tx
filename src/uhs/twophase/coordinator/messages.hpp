// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COORDINATOR_MESSAGES_H_
#define OPENCBDC_TX_SRC_COORDINATOR_MESSAGES_H_

#include "uhs/transaction/transaction.hpp"

namespace cbdc::coordinator::rpc {
    /// Coordinator RPC request message; a compact transaction.
    using request = transaction::compact_tx;
    /// Coordinator RPC response message; a boolean, true if the coordinator
    /// completed the transaction, false otherwise.
    using response = bool;
}

#endif // OPENCBDC_TX_SRC_COORDINATOR_MESSAGES_H_
