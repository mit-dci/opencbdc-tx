// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_AGENT_RUNNERS_EVM_RLP_H_
#define OPENCBDC_TX_SRC_3PC_AGENT_RUNNERS_EVM_RLP_H_

#include "format.hpp"
#include "util/common/buffer.hpp"
#include "util/serialization/buffer_serializer.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/serializer.hpp"
#include "util/serialization/util.hpp"

#include <evmc/evmc.hpp>
#include <optional>
#include <vector>

namespace cbdc {
    /// Possible types for an RLP value
    enum class rlp_value_type {
        /// A collection of RLP values
        array,
        /// A singular RLP value (byte array)
        buffer,
    };

    /// This class contains a value that can be serialized into,
    /// or was deserialized from, a Recursive Length Prefix (RLP)
    /// encoded representation.
    class rlp_value {
      public:
        /// Default constructor. Sets m_type to rlp_value_type::buffer
        rlp_value();

        /// Constructor.
        /// \param type The type of RLP value to be constructed
        explicit rlp_value(rlp_value_type type);

        /// Constructs an RLP value of type rlp_value_type::buffer and
        /// assigns the given data to its internal buffer
        /// \param data The data to be assigned to the internal buffer
        explicit rlp_value(const buffer& data);

        /// Assigns the given data to the internal buffer of this rlp_value
        /// can only be used for rlp_value instances of type
        /// rlp_value_type::buffer
        /// \param data The data to be assigned to the internal buffer
        void assign(const buffer& data);

        /// Pushes an rlp_value into an rlp_value of type rlp_value_type::array
        /// You can push both rlp_value_type::buffer and rlp_value_type::array
        /// \param val The rlp_value to append to the end of the array
        /// \returns true on success, false if the rlp_value is of the wrong type
        auto push_back(const rlp_value& val) -> bool;

        /// Serializes the rlp_value in RLP representation into the passed
        /// serializer
        /// \param ser Serializer instance to write the RLP value to
        void write_to(serializer& ser) const;

        /// Deserializes the rlp_value in RLP representation from the passed
        /// serializer into the current rlp_value instance
        /// \param ser Serializer instance to read the RLP value from
        void read_from(serializer& ser);

        /// Returns the rlp_value at the given index for RLP values of type
        /// rlp_value_type::array
        /// \param idx the index in the array
        /// \return the RLP value at that index
        [[nodiscard]] auto value_at(size_t idx) const -> rlp_value;

        /// Get the size of the rlp_value
        /// \return size of the buffer for rlp_value of type
        /// rlp_value_type::buffer, or the number of items in the array for
        /// rlp_value of type rlp_value_type::array
        [[nodiscard]] auto size() const -> size_t;

        /// Get the type of rlp_value
        /// \return type of the rlp_value
        [[nodiscard]] auto type() const -> rlp_value_type;

        /// Returns a raw pointer to the start of the buffer data for rlp_value
        /// of type rlp_buffer.
        /// \return a pointer to the data
        [[nodiscard]] auto data() const -> const void*;

        /// Return RLP value as address or byte array.
        /// \tparam address or byte array type.
        /// \return byte array or address.
        template<typename T>
        [[nodiscard]] auto value() const -> typename std::enable_if_t<
            std::is_same<T, evmc::bytes32>::value
                || std::is_same<T, evmc::address>::value,
            T> {
            auto res = T();
            auto buf = cbdc::buffer();
            buf.extend(sizeof(res.bytes));
            std::memcpy(buf.data_at(sizeof(res.bytes) - m_buffer.size()),
                        m_buffer.data(),
                        m_buffer.size());
            std::memcpy(&res.bytes, buf.data(), buf.size());
            return res;
        }

      private:
        void write_array_to(serializer& ser) const;
        void write_buffer_to(serializer& ser) const;
        void read_buffer_from(serializer& ser, size_t size);
        void read_array_from(serializer& ser, size_t size);

        buffer m_buffer{};
        std::vector<rlp_value> m_values{};
        rlp_value_type m_type{};
    };

    /// Turns an existing value into an rlp_value by first serializing it as
    /// a cbdc::buffer, and then turning that into an rlp_value.
    /// \param obj object to serialize and wrap in rlp_value
    /// \param trim_leading_zeroes if true, removes leading 0x00 bytes in the
    /// resulting sequence after making a cbdc::buffer out of the passed obj
    /// before turning it into an rlp_value
    /// \return rlp_value of type buffer with the passed object as contents
    template<typename T>
    auto make_rlp_value(const T& obj, bool trim_leading_zeroes = false)
        -> rlp_value {
        auto pkt = make_buffer(obj);
        if(trim_leading_zeroes) {
            size_t start_idx = 0;
            std::byte b{};
            auto sz = pkt.size();
            while(start_idx < sz) {
                std::memcpy(&b, pkt.data_at(start_idx), 1);
                if(b != std::byte(0)) {
                    break;
                }
                start_idx++;
            }

            auto buf = cbdc::buffer();
            buf.extend(sz - start_idx);
            std::memcpy(buf.data(), pkt.data_at(start_idx), buf.size());
            return rlp_value(buf);
        }
        return rlp_value(pkt);
    }

    /// Turns multiple rlp_value objects into an rlp_value of type array
    /// \param values values to add to the array
    /// \return rlp_value of type array with the passed objects as contents
    template<typename... Args>
    auto make_rlp_array(const Args&... values) -> rlp_value {
        std::vector<rlp_value> vec = {values...};
        auto val = rlp_value(rlp_value_type::array);
        for(const auto& v : vec) {
            val.push_back(v);
        }
        return val;
    }

    /// Serializes the passed len from the given offset as RLP compatible size
    /// representation as documented in https://eth.wiki/fundamentals/rlp
    /// \param ser serializer to write the result to
    /// \param len length to serialize
    /// \param offset offset to base the representation on. In RLP the
    /// offset distinguishes between a value or an array
    void
    serialize_rlp_length(serializer& ser, size_t len, unsigned char offset);

    /// Creates a binary representation for sizes that exceed the single-byte
    /// presentation
    /// \param size size value to serialize
    /// \return vector of bytes representing the passed in size
    auto serialize_size(size_t size) -> std::vector<std::byte>;

    /// RLP encodes an access list
    /// \param access_list the access list to encode
    /// \return rlp_value of type rlp_value_array with the access list as contents
    auto rlp_encode_access_list(
        const threepc::agent::runner::evm_access_list& access_list)
        -> rlp_value;

    /// Decodes an access list from and rlp_value of type rlp_value_type::array
    /// \param rlp rlp_value to decode from
    /// \return evm_access_list that was decoded or std::nullopt on failure
    auto rlp_decode_access_list(const rlp_value& rlp)
        -> std::optional<threepc::agent::runner::evm_access_list>;

    /// Decodes a binary representation for sizes that exceed the single-byte
    /// presentation into size_t
    /// \param buf buffer containing the binary representation to decode
    /// \return size_t that was decoded
    auto deserialize_size(const cbdc::buffer& buf) -> size_t;

    auto operator<<(serializer& ser, const rlp_value& v) -> serializer&;

    auto operator>>(serializer& ser, cbdc::rlp_value& v) -> serializer&;
}

#endif // OPENCBDC_TX_SRC_3PC_AGENT_RUNNERS_EVM_RLP_H_
