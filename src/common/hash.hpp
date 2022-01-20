// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COMMON_HASH_H_
#define OPENCBDC_TX_SRC_COMMON_HASH_H_

#include "crypto/siphash.h"

#include <array>
#include <sstream>

namespace cbdc {
    /// The size of the hashes used throughout the system, in bytes.
    static constexpr const int hash_size = 32;

    /// SHA256 hash container.
    using hash_t = std::array<unsigned char, cbdc::hash_size>;

    /// Converts a hash to a hexadecimal string.
    /// \param val hash to convert.
    /// \return hex representation of the hash.
    auto to_string(const hash_t& val) -> std::string;

    /// Parses a hexadecimal representation of a hash.
    /// \param val string with a hex representation of a hash.
    /// \return hash value of the string.
    auto hash_from_hex(const std::string& val) -> hash_t;

    /// Calculates the SHA256 hash of the specified data.
    /// \param data byte array containing data to hash.
    /// \param len the number of bytes of the data to hash.
    /// \return the hash of the data.
    auto hash_data(const std::byte* data, size_t len) -> hash_t;
}

#endif // OPENCBDC_TX_SRC_COMMON_HASH_H_
