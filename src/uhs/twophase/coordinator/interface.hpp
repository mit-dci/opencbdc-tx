// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COORDINATOR_INTERFACE_H_
#define OPENCBDC_TX_SRC_COORDINATOR_INTERFACE_H_

#include "uhs/transaction/transaction.hpp"

#include <functional>

namespace cbdc::coordinator {
    /// \brief Interface for a coordinator.
    /// Provides consistent semantics whether using a remote coordinator via
    /// an RPC client, or a local implementation directly. An RPC server can
    /// use this interface to handle requests without knowing how the interface
    /// is implemented. Allows for easier mocking in test suites and swapping
    /// implementations without changing dependent components.
    class interface {
      public:
        virtual ~interface() = default;
        interface() = default;
        interface(const interface&) = delete;
        auto operator=(const interface&) -> interface& = delete;
        interface(interface&&) = delete;
        auto operator=(interface&&) -> interface& = delete;

        /// Signature of callback function for a transaction execution result.
        using callback_type = std::function<void(std::optional<bool>)>;

        /// Execute the given compact transaction. An RPC client subclass would
        /// send a request to a remote coordinator and wait for the response. A
        /// coordinator implementation would coordinate the transaction between
        /// locking shards and return the execution result.
        /// \param tx transaction to execute.
        /// \param result_callback function to call when the transaction has
        ///                        executed to completion or failed.
        /// \return true if the implementation started executing the
        ///         transaction.
        virtual auto execute_transaction(transaction::compact_tx tx,
                                         callback_type result_callback) -> bool
            = 0;
    };
}

#endif
