// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash.hpp"

#include <cstring>
#include <ethash/keccak.hpp>
#include <iomanip>
#include <vector>

namespace cbdc {
    auto keccak_data(const void* data, size_t len) -> hash_t {
        hash_t ret{};
        auto data_vec = std::vector<unsigned char>(len);
        std::memcpy(data_vec.data(), data, len);
        auto resp = ethash::keccak256(data_vec.data(), data_vec.size());
        std::memcpy(ret.data(), &resp, sizeof(resp));
        return ret;
    }
}
