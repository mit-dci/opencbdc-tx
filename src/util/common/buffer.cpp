// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "buffer.hpp"

#include <array>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace cbdc {
    void buffer::clear() {
        m_data.clear();
    }

    void buffer::append(const void* data, size_t len) {
        const auto orig_size = m_data.size();
        m_data.resize(orig_size + len);
        std::memcpy(&m_data[orig_size], data, len);
    }

    auto buffer::size() const -> size_t {
        return m_data.size();
    }

    auto buffer::data() -> void* {
        return m_data.data();
    }

    auto buffer::data() const -> const void* {
        return m_data.data();
    }

    auto buffer::data_at(size_t offset) -> void* {
        return &m_data[offset];
    }

    auto buffer::data_at(size_t offset) const -> const void* {
        return &m_data[offset];
    }

    auto buffer::operator==(const buffer& other) const -> bool {
        return m_data == other.m_data;
    }

    void buffer::extend(size_t len) {
        m_data.resize(m_data.size() + len);
    }

    auto buffer::c_ptr() const -> const unsigned char* {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<const unsigned char*>(m_data.data());
    }

    auto buffer::c_str() const -> const char* {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<const char*>(m_data.data());
    }

    auto buffer::to_hex() const -> std::string {
        // TODO: refactor these into general hex conversion functions with
        //       functions in hash.cpp.
        std::stringstream ret;
        ret << std::hex << std::setfill('0');

        for(const auto& byte : m_data) {
            // TODO: This function is likely very slow. At some point this
            //       could be converted to lookup table that we make ourselves.
            ret << std::setw(2) << static_cast<int>(byte);
        }

        return ret.str();
    }

    auto
    buffer::to_hex_prefixed(const std::string& prefix) const -> std::string {
        auto res = std::string();
        res.append(prefix);
        res.append(to_hex());
        return res;
    }

    auto buffer::from_hex(const std::string& hex) -> std::optional<buffer> {
        constexpr auto max_size = 102400;
        if(hex.empty() || ((hex.size() % 2) != 0) || (hex.size() > max_size)) {
            return std::nullopt;
        }

        auto ret = cbdc::buffer();

        for(size_t i = 0; i < hex.size(); i += 2) {
            unsigned int v{};
            std::stringstream s;
            // TODO: This function is likely very slow. At some point this
            //       could be converted to lookup table that we make ourselves.
            s << std::hex << hex.substr(i, 2);
            if(!(s >> v)) {
                return std::nullopt;
            }
            ret.m_data.push_back(static_cast<std::byte>(v));
        }

        return ret;
    }

    auto buffer::from_hex_prefixed(const std::string& hex,
                                   const std::string& prefix)
        -> std::optional<buffer> {
        size_t offset = 0;
        if(hex.rfind(prefix, 0) == 0) {
            offset = prefix.size();
        }
        auto hex_str = hex.substr(offset);
        if(hex_str.size() % 2 != 0) {
            hex_str.insert(0, "0");
        }
        return from_hex(hex_str);
    }
}
