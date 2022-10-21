// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_AGENT_RUNNERS_EVM_HASH_H_
#define OPENCBDC_TX_SRC_3PC_AGENT_RUNNERS_EVM_HASH_H_

#include <array>
#include <sstream>
#include <util/common/hash.hpp>

namespace cbdc {
    /// Calculates the Keccak256 hash of the specified data.
    /// \param data byte array containing data to hash.
    /// \param len the number of bytes of the data to hash.
    /// \return the hash of the data.
    auto keccak_data(const void* data, size_t len) -> hash_t;
}

#endif // OPENCBDC_TX_SRC_3PC_AGENT_RUNNERS_EVM_HASH_H_
