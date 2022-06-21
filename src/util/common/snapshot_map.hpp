// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CBDC_UNIVERSE0_SRC_COMMON_SNAPSHOT_MAP_H_
#define CBDC_UNIVERSE0_SRC_COMMON_SNAPSHOT_MAP_H_

#include <cassert>
#include <map>
#include <set>

namespace cbdc {
    template<typename K, typename V, typename... Ts>
    class snapshot_map {
      public:
        using map_type = std::map<K, V, Ts...>;
        using set_type = std::set<K, Ts...>;

        auto operator=(map_type&& m) -> snapshot_map<K, V, Ts...>& {
            m_map = std::move(m);
            return *this;
        }

        void release_snapshot() {
            m_snapshot = false;
        }

        void snapshot() {
            m_snapshot = true;
        }

        [[nodiscard]] auto find(const K& key) const ->
            typename map_type::const_iterator {
            auto added_it = m_added.find(key);
            if(added_it != m_added.end()) {
                return added_it;
            }

            auto removed_it = m_removed.find(key);
            if(removed_it != m_removed.end()) {
                return m_map.end();
            }

            return m_map.find(key);
        }

        [[nodiscard]] auto size() const -> size_t {
            return m_added.size() + m_removed.size() + m_map.size();
        }

        [[nodiscard]] auto end() const -> typename map_type::const_iterator {
            return m_map.end();
        }

        [[nodiscard]] auto begin() const -> typename map_type::const_iterator {
            return m_map.begin();
        }

        template<class... Args>
        auto emplace(Args&&... args)
            -> std::pair<typename map_type::iterator, bool> {
            gc();
            auto ret = [&]() {
                if(m_snapshot) {
                    return m_added.emplace(std::forward<Args>(args)...);
                }
                auto it = m_map.emplace(std::forward<Args>(args)...);
                m_added.erase(it.first->first);
                return it;
            }();
            m_removed.erase(ret.first->first);
            return ret;
        }

        auto erase(typename map_type::const_iterator it) ->
            typename map_type::iterator {
            assert(!m_snapshot);
            m_added.erase(it->first);
            m_removed.erase(it->first);
            return m_map.erase(it);
        }

        void erase(const K& key) {
            gc();
            m_added.erase(key);
            if(m_snapshot) {
                m_removed.emplace(key);
            } else {
                m_removed.erase(key);
                m_map.erase(key);
            }
        }

      private:
        map_type m_map;
        map_type m_added;
        set_type m_removed;
        bool m_snapshot{false};

        void gc() {
            if(m_snapshot) {
                return;
            }
            constexpr size_t factor = 1000000;
            constexpr size_t one = 1;
            auto added_elems = std::min(std::max(m_added.size() / factor, one),
                                        m_added.size());
            size_t count = 0;
            for(auto it = m_added.begin();
                it != m_added.end() && count < added_elems;
                count++) {
                auto n = m_added.extract(it++);
                m_map.insert(std::move(n));
            }
            count = 0;
            auto removed_elems
                = std::min(std::max(m_removed.size() / factor, one),
                           m_removed.size());
            for(auto it = m_removed.begin();
                it != m_removed.end() && count < removed_elems;
                count++) {
                auto n = m_removed.extract(it++);
                m_map.erase(n.value());
            }
        }
    };
}

#endif
