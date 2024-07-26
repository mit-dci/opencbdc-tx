// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_FORMAT_H_
#define OPENCBDC_TX_SRC_RPC_FORMAT_H_

#include "messages.hpp"
#include "util/serialization/serializer.hpp"

namespace cbdc {
    auto operator<<(serializer& ser, const rpc::header& header) -> serializer&;
    auto operator>>(serializer& deser, rpc::header& header) -> serializer&;

    template<typename T>
    auto operator<<(serializer& ser,
                    const rpc::request<T>& req) -> serializer& {
        return ser << req.m_header << req.m_payload;
    }

    template<typename T>
    auto operator>>(serializer& deser, rpc::request<T>& req) -> serializer& {
        return deser >> req.m_header >> req.m_payload;
    }

    template<typename T>
    auto operator<<(serializer& ser,
                    const rpc::response<T>& resp) -> serializer& {
        return ser << resp.m_header << resp.m_payload;
    }

    template<typename T>
    auto operator>>(serializer& deser, rpc::response<T>& resp) -> serializer& {
        return deser >> resp.m_header >> resp.m_payload;
    }
}

#endif
