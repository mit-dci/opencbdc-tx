// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "interface.hpp"

#include "util/common/config.hpp"

#include <utility>

namespace cbdc::locking_shard {
    interface::interface(std::pair<uint8_t, uint8_t> output_range)
        : m_output_range(std::move(output_range)) {}

    auto interface::hash_in_shard_range(const hash_t& h) const -> bool {
        return config::hash_in_shard_range(m_output_range, h);
    }

    auto tx::operator==(const tx& rhs) const -> bool {
        return m_tx == rhs.m_tx;
    }
}
