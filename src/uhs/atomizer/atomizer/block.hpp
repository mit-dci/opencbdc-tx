// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_ATOMIZER_BLOCK_H_
#define OPENCBDC_TX_SRC_ATOMIZER_BLOCK_H_

#include "uhs/transaction/transaction.hpp"
#include "util/common/buffer.hpp"

#include <cassert>
#include <cstddef>
#include <limits>
#include <vector>

namespace cbdc::atomizer {
    /// Batch of compact transactions settled by the atomizer.
    struct block {
        auto operator==(const block& rhs) const -> bool;

        /// Index of this block in the overall contiguous sequence of blocks
        /// from the first block starting at height zero.
        uint64_t m_height{};
        /// Compact transactions settled by the atomizer in this block.
        std::vector<transaction::compact_tx> m_transactions;
    };
}

#endif // OPENCBDC_TX_SRC_ATOMIZER_BLOCK_H_
