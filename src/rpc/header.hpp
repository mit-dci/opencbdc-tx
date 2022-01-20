// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_HEADER_H_
#define OPENCBDC_TX_SRC_RPC_HEADER_H_

#include <cstdint>

namespace cbdc::rpc {
    using request_id_type = uint64_t;

    /// RPC request and response header.
    struct header {
        /// Identifier for matching requests with responses.
        request_id_type m_request_id;
    };
}

#endif
