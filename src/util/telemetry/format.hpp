// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_TELEMETRY_FORMAT_H_
#define OPENCBDC_TX_SRC_TELEMETRY_FORMAT_H_

#include "util/common/buffer.hpp"
#include "util/common/config.hpp"
#include "util/serialization/serializer.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace cbdc {

    /// Serializes the std::string.
    auto operator<<(serializer& packet, std::string s) -> serializer&;

    /// \brief Deserializes a std::string
    ///
    /// \see \ref cbdc::operator<<(serializer&, std::string)
    auto operator>>(serializer& packet, std::string& s) -> serializer&;
}

#endif // OPENCBDC_TX_SRC_TELEMETRY_FORMAT_H_
