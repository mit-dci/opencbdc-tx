// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "math.hpp"

#include <limits>

// Until std::span is available in C++20, we don't have a bounds-checked way
// to access the data behind evmc::uint256be.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
namespace cbdc::parsec::agent::runner {
    auto operator+(const evmc::uint256be& lhs,
                   const evmc::uint256be& rhs) -> evmc::uint256be {
        auto ret = evmc::uint256be{};
        auto tmp = uint64_t{};
        auto carry = uint8_t{};
        constexpr uint64_t max_val = std::numeric_limits<uint8_t>::max();
        for(int i = sizeof(lhs.bytes) - 1; i >= 0; i--) {
            tmp = lhs.bytes[i] + rhs.bytes[i] + carry;
            carry = static_cast<unsigned char>(tmp > max_val);
            ret.bytes[i] = (tmp & max_val);
        }
        return ret;
    }

    auto operator-(const evmc::uint256be& lhs,
                   const evmc::uint256be& rhs) -> evmc::uint256be {
        auto ret = evmc::uint256be{};
        auto tmp1 = uint64_t{};
        auto tmp2 = uint64_t{};
        auto res = uint64_t{};
        auto borrow = uint8_t{};
        constexpr uint64_t max_val = std::numeric_limits<uint8_t>::max();
        for(int i = sizeof(lhs.bytes) - 1; i >= 0; i--) {
            tmp1 = lhs.bytes[i] + (max_val + 1);
            tmp2 = rhs.bytes[i] + borrow;
            res = tmp1 - tmp2;
            ret.bytes[i] = (res & max_val);
            borrow = static_cast<unsigned char>(res <= max_val);
        }
        return ret;
    }

    auto operator*(const evmc::uint256be& lhs,
                   const evmc::uint256be& rhs) -> evmc::uint256be {
        auto ret = evmc::uint256be{};
        for(size_t i = 0; i < sizeof(lhs.bytes); i++) {
            auto row = evmc::uint256be{};
            if(lhs.bytes[i] == 0) {
                continue;
            }
            for(size_t j = 0; j < sizeof(rhs.bytes); j++) {
                if(rhs.bytes[j] == 0) {
                    continue;
                }
                auto shift = (sizeof(lhs.bytes) - i - 1)
                           + (sizeof(rhs.bytes) - j - 1);
                if(shift >= sizeof(ret.bytes)) {
                    continue;
                }
                auto intermediate
                    = static_cast<uint64_t>(lhs.bytes[i] * rhs.bytes[j]);
                auto tmp = evmc::uint256be(intermediate);
                tmp = tmp << static_cast<size_t>(shift);
                row = row + tmp;
            }
            ret = ret + row;
        }
        return ret;
    }

    auto operator<<(const evmc::uint256be& lhs,
                    size_t count) -> evmc::uint256be {
        auto ret = evmc::uint256be{};
        if(count >= sizeof(lhs.bytes)) {
            return ret;
        }
        for(size_t i = 0; i + count < sizeof(lhs.bytes); i++) {
            ret.bytes[i] = lhs.bytes[i + count];
        }
        return ret;
    }
}
// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
