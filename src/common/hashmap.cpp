// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hashmap.hpp"

namespace cbdc::hashing {
    auto null::operator()(const hash_t& hash) const noexcept -> size_t {
        size_t ret{};
        std::memcpy(&ret, hash.data(), sizeof(ret));
        return ret;
    }
}
