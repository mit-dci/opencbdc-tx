// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RAFT_UTIL_H_
#define OPENCBDC_TX_SRC_RAFT_UTIL_H_

#include "serialization.hpp"
#include "serialization/util.hpp"

namespace cbdc {
    /// Serialize object into nuraft::buffer using a cbdc::nuraft_serializer.
    /// \tparam T type of object to serialize.
    /// \tparam B type of buffer to return, must be nuraft::ptr<nuraft::buffer>>
    ///         for this template to be enabled.
    /// \param obj object to serialize.
    /// \return a serialized buffer of the object.
    template<typename T, typename B>
    auto make_buffer(const T& obj)
        -> std::enable_if_t<std::is_same_v<B, nuraft::ptr<nuraft::buffer>>,
                            nuraft::ptr<nuraft::buffer>> {
        auto pkt = nuraft::buffer::alloc(cbdc::serialized_size(obj));
        auto ser = cbdc::nuraft_serializer(*pkt);
        ser << obj;
        return pkt;
    }

    /// Deserialize object of given type from a nuraft::buffer.
    /// \tparam T type of object to deserialize from the buffer.
    /// \param buf buffer from which to deserialize the object.
    /// \return deserialized object, or std::nullopt if the deserialization
    ///         failed.
    template<typename T>
    auto from_buffer(nuraft::buffer& buf) -> std::optional<T> {
        auto deser = cbdc::nuraft_serializer(buf);
        T ret{};
        if(!(deser >> ret)) {
            return std::nullopt;
        }
        return ret;
    }
}

#endif
