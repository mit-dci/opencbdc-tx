// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RAFT_INDEX_COMPARATOR_H_
#define OPENCBDC_TX_SRC_RAFT_INDEX_COMPARATOR_H_

#include <leveldb/comparator.h>

namespace cbdc::raft {
    /// LevelDB comparator for ordering NuRaft log indices.
    class index_comparator : public leveldb::Comparator {
      public:
        /// Compare the order of the two given LevelDB keys when converted to
        /// uint64_t.
        /// \param a first key.
        /// \param b second key.
        /// \return 0 if a == b. -1 if a < b. 1 if a > b.
        [[nodiscard]] auto Compare(const leveldb::Slice& a,
                                   const leveldb::Slice& b) const
            -> int override;

        /// Return the comparator name.
        /// \return "IndexComparator".
        [[nodiscard]] auto Name() const -> const char* override;

        /// Not implemented.
        void FindShortestSeparator(
            std::string* /* start */,
            const leveldb::Slice& /* limit */) const override;

        /// Not implemented.
        void FindShortSuccessor(std::string* /* key */) const override;
    };
}

#endif // OPENCBDC_TX_SRC_RAFT_INDEX_COMPARATOR_H_
