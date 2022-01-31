// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SERIALIZATION_UTIL_H_
#define OPENCBDC_TX_SRC_SERIALIZATION_UTIL_H_

#include "buffer_serializer.hpp"
#include "size_serializer.hpp"

#include <memory>

namespace cbdc {
    /// Calculates the serialized size in bytes of the given object when
    /// serialized using \ref serializer. \see \ref size_serializer.
    /// \tparam T type of object.
    /// \param obj object to serialize.
    /// \return serialized size in bytes.
    template<typename T>
    auto serialized_size(const T& obj) -> size_t {
        auto ser = size_serializer();
        ser << obj;
        return ser.size();
    }

    /// Serialize object into cbdc::buffer using a cbdc::buffer_serializer.
    /// \tparam T type of object to serialize.
    /// \tparam B type of buffer to return, must be cbdc::buffer for this
    ///         template to be enabled.
    /// \return a serialized buffer of the object.
    template<typename T, typename B = buffer>
    auto make_buffer(const T& obj)
        -> std::enable_if_t<std::is_same_v<B, buffer>, cbdc::buffer> {
        // TODO: we could use serialized_size to preallocate the buffer which
        //       could improve performance.
        auto pkt = cbdc::buffer();
        auto ser = cbdc::buffer_serializer(pkt);
        ser << obj;
        return pkt;
    }

    /// Serialize object into std::shared_ptr<cbdc::buffer> using a
    /// cbdc::buffer_serializer.
    /// \tparam T type of object to serialize.
    /// \return a shared_ptr to a serialized buffer of the object.
    template<typename T>
    auto make_shared_buffer(const T& obj) -> std::shared_ptr<cbdc::buffer> {
        auto buf = std::make_shared<cbdc::buffer>();
        auto ser = cbdc::buffer_serializer(*buf);
        ser << obj;
        return buf;
    }

    /// Deserialize object of given type from a cbdc::buffer.
    /// \tparam T type of object to deserialize from the buffer.
    /// \param buf buffer from which to deserialize the object.
    /// \return deserialized object, or std::nullopt if the deserialization
    ///         failed.
    template<typename T>
    auto from_buffer(cbdc::buffer& buf) -> std::optional<T> {
        auto deser = cbdc::buffer_serializer(buf);
        T ret{};
        if(!(deser >> ret)) {
            return std::nullopt;
        }
        return ret;
    }
}

#endif // OPENCBDC_TX_SRC_SERIALIZATION_UTIL_H_
