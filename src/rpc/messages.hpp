// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_MESSAGES_H_
#define OPENCBDC_TX_SRC_RPC_MESSAGES_H_

#include "header.hpp"

#include <optional>

namespace cbdc::rpc {
    /// RPC request message.
    /// \tparam T request payload type.
    template<typename T>
    struct request {
        /// Request header.
        header m_header;
        /// Request payload.
        T m_payload;
    };

    /// RPC response message.
    /// \tparam T response payload type.
    template<typename T>
    struct response {
        /// Response header.
        header m_header{};
        /// Response payload or std::nullopt if processing the request failed.
        std::optional<T> m_payload;
    };
}

#endif
