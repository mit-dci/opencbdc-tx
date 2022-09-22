// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "impl.hpp"

namespace cbdc::threepc::directory {
    impl::impl(size_t n_shards) : m_n_shards(n_shards) {}

    auto impl::key_location(runtime_locking_shard::key_type key,
                            key_location_callback_type result_callback)
        -> bool {
        auto key_hash = m_siphash(key);
        auto shard = key_hash % m_n_shards;
        result_callback(shard);
        return true;
    }
}
