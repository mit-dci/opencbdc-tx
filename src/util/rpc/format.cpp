// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "format.hpp"

#include "util/serialization/format.hpp"

namespace cbdc {
    auto operator<<(serializer& ser, const rpc::header& header)
        -> serializer& {
        return ser << header.m_request_id;
    }

    auto operator>>(serializer& deser, rpc::header& header) -> serializer& {
        return deser >> header.m_request_id;
    }
}
