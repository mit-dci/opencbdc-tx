// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "format.hpp"

#include "util/serialization/format.hpp"

namespace cbdc {
    auto operator<<(serializer& ser, const threepc::agent::rpc::request& req)
        -> serializer& {
        return ser << req.m_function << req.m_param << req.m_dry_run;
    }

    auto operator>>(serializer& deser, threepc::agent::rpc::request& req)
        -> serializer& {
        return deser >> req.m_function >> req.m_param >> req.m_dry_run;
    }
}
