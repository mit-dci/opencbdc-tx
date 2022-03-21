// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash.hpp"

#include "crypto/sha3.h"

#include <cstring>
#include <iomanip>
#include <vector>

namespace cbdc {
    auto to_string(const hash_t& val) -> std::string {
        std::stringstream ret;
        ret << std::hex << std::setfill('0');

        for(const auto& byte : val) {
            ret << std::setw(2) << static_cast<int>(byte);
        }

        return ret.str();
    }

    auto hash_from_hex(const std::string& val) -> hash_t {
        hash_t ret;

        for(size_t i = 0; i < val.size(); i += 2) {
            unsigned int v{};
            std::stringstream s;
            s << std::hex << val.substr(i, 2);
            s >> v;
            ret[i / 2] = static_cast<uint8_t>(v);
        }

        return ret;
    }

    auto hash_data(const std::byte* data, size_t len) -> hash_t {
        hash_t ret;
        SHA3_256 sha;

        auto data_vec = std::vector<unsigned char>(len);
        std::memcpy(data_vec.data(), data, len);
        sha.Write(Span{static_cast<unsigned char*>(data_vec.data()), len});
        sha.Finalize(ret);

        return ret;
    }
}
