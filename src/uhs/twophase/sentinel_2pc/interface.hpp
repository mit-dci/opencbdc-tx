// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SENTINEL_2PC_INTERFACE_H_
#define OPENCBDC_TX_SRC_SENTINEL_2PC_INTERFACE_H_

#include "uhs/sentinel/interface.hpp"
#include "uhs/transaction/validation.hpp"
#include "util/common/hash.hpp"

#include <optional>
#include <string>

namespace cbdc::sentinel {
    /// Interface for an asynchronous sentinel.
    class async_interface {
      public:
        virtual ~async_interface() = default;

        async_interface() = default;
        async_interface(const async_interface&) = delete;
        auto operator=(const async_interface&) -> async_interface& = delete;
        async_interface(async_interface&&) = delete;
        auto operator=(async_interface&&) -> async_interface& = delete;

        /// Callback function for transaction execution result.
        using result_callback_type
            = std::function<void(std::optional<cbdc::sentinel::response>)>;

        /// Validate transaction on the sentinel, forward it to the coordinator
        /// network, and return the execution result using a callback function.
        /// \param tx transaction to execute.
        /// \param result_callback function to call with execution result.
        /// \return false if the implementation could not start processing the
        ///         transaction.
        virtual auto execute_transaction(transaction::full_tx tx,
                                         result_callback_type result_callback)
            -> bool
            = 0;
    };
}

#endif
