// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "random_source.hpp"

#include <cassert>
#include <cstring>
#include <fstream>
#include <random>

namespace cbdc {
    random_source::random_source(const std::string& source_file) {
        std::ifstream m_source(source_file, std::ios::in | std::ios::binary);
        assert(m_source.good());

        std::array<char, std::tuple_size<hash_t>::value> dest{};
        m_source.read(dest.data(), dest.size());
        assert(m_source.gcount() == static_cast<std::streamsize>(dest.size()));
        std::array<unsigned char, sizeof(dest)> dest_unsigned{};
        std::memcpy(dest_unsigned.data(), dest.data(), dest.size());
        m_sha.Write(Span{static_cast<unsigned char*>(dest_unsigned.data()),
                         dest_unsigned.size()});
    }

    auto random_source::operator()() -> result_type {
        std::unique_lock<std::mutex> l(m_mut);
        result_type ret{};
        std::array<unsigned char, sizeof(ret)> ret_arr{};
        for(auto& v : ret_arr) {
            if(m_buf.empty()) {
                auto h = hash_at_index(m_counter++);
                for(auto b : h) {
                    m_buf.push(b);
                }
            }
            v = m_buf.front();
            m_buf.pop();
        }
        std::memcpy(&ret, ret_arr.data(), ret_arr.size());
        return ret;
    }

    auto random_source::random_hash() -> hash_t {
        hash_t ret{};
        std::uniform_int_distribution<unsigned char> gen;
        for(auto&& b : ret) {
            b = gen(*this);
        }
        return ret;
    }

    auto random_source::hash_at_index(uint64_t idx) -> hash_t {
        hash_t ret;
        std::array<unsigned char, sizeof(idx)> idx_arr{};
        std::memcpy(idx_arr.data(), &idx, sizeof(idx));
        m_sha.Write(
            Span{static_cast<unsigned char*>(idx_arr.data()), sizeof(idx)});
        m_sha.Finalize(ret);
        return ret;
    }
}
