// Copyright (c) 2021 MIT Digital Currency Initiative
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CACHE_SET_H_INC
#define CACHE_SET_H_INC

#include <cassert>
#include <queue>
#include <shared_mutex>
#include <unordered_set>

namespace cbdc {
    /// \brief Thread-safe set with a maximum size.
    ///
    /// If full, inserting a new value will evict the oldest value.
    /// \tparam K type of the values in the set.
    /// \tparam H hasher compatible with std::unordered_set.
    template<typename K, typename H = std::hash<K>>
    class cache_set {
      public:
        cache_set() = delete;

        /// Constructor.
        /// \param max_size maximum number of elements in the set.
        explicit cache_set(size_t max_size) : m_max_size(max_size) {
            m_vals.max_load_factor(std::numeric_limits<float>::max());
            static constexpr auto buckets_factor = 2;
            m_vals.rehash(buckets_factor * max_size);
        };

        /// Adds a value to the set, evicting the oldest value if the set is
        /// full.
        /// \param val value to add.
        /// \return true if the value was not already in the set.
        template<typename T>
        auto add(T&& val) -> bool {
            std::unique_lock<std::shared_mutex> l(m_mut);
            auto added = m_vals.emplace(std::forward<T>(val));
            if(added.second) {
                m_eviction_queue.push(std::ref(*added.first));
                if(m_eviction_queue.size() >= m_max_size) {
                    auto& v = m_eviction_queue.front();
                    m_vals.erase(v);
                    m_eviction_queue.pop();
                }
            }
            assert(m_eviction_queue.size() <= m_max_size);
            return added.second;
        }

        /// Determines whether a given value is present in the cache set.
        /// \param val value to check.
        /// \return true if the value is present in the set.
        [[nodiscard]] auto contains(const K& val) const -> bool {
            std::shared_lock<std::shared_mutex> l(m_mut);
            return m_vals.find(val) != m_vals.end();
        }

      private:
        std::unordered_set<K, H> m_vals;
        std::queue<std::reference_wrapper<const K>> m_eviction_queue;
        size_t m_max_size;
        mutable std::shared_mutex m_mut;
    };
}

#endif
