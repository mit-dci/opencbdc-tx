// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "index_comparator.hpp"

#include <cstdint>
#include <cstring>
#include <cstdint>
#include <leveldb/slice.h>

namespace cbdc::raft {
    auto index_comparator::Compare(const leveldb::Slice& a,
                                   const leveldb::Slice& b) const -> int {
        assert(a.size() == b.size());
        uint64_t a_val{0};
        uint64_t b_val{0};
        std::memcpy(&a_val, a.data(), sizeof(a_val));
        std::memcpy(&b_val, b.data(), sizeof(b_val));
        if(a_val == b_val) {
            return 0;
        }

        if(a_val < b_val) {
            return -1;
        }

        return 1;
    }

    auto index_comparator::Name() const -> const char* {
        return "IndexComparator";
    }

    void index_comparator::FindShortestSeparator(
        std::string* /* start */,
        const leveldb::Slice& /* limit */) const {}

    void index_comparator::FindShortSuccessor(std::string* /* key */) const {}
}
