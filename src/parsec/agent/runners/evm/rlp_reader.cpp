#include "rlp.hpp"

namespace cbdc {

    auto operator>>(serializer& ser, cbdc::rlp_value& v) -> serializer& {
        v.read_from(ser);
        return ser;
    }

    void rlp_value::read_from(serializer& ser) {
        static constexpr unsigned char byte_size_offset = 0x80;
        static constexpr unsigned char array_size_offset = 0xc0;
        static constexpr unsigned char max_onebyte_length = 55;
        unsigned char b{};
        ser.read(&b, 1);

        m_buffer.clear();
        m_values.clear();

        if(b < byte_size_offset) {
            m_type = rlp_value_type::buffer;
            m_buffer.clear();
            m_buffer.extend(1);
            std::memcpy(m_buffer.data(), &b, 1);
        } else if(b >= byte_size_offset
                  && b <= byte_size_offset + max_onebyte_length) {
            auto read_len = static_cast<size_t>(b - byte_size_offset);
            read_buffer_from(ser, read_len);
        } else if(b > byte_size_offset + max_onebyte_length
                  && b < array_size_offset) {
            // First we read the length of the length
            auto len_len = static_cast<size_t>(b - byte_size_offset
                                               - max_onebyte_length);
            // Then we read the length bytes
            auto len_buf = cbdc::buffer();
            len_buf.extend(len_len);
            ser.read(len_buf.data(), len_len);
            // Then we turn the read length into a size_t
            auto read_len = deserialize_size(len_buf);
            read_buffer_from(ser, read_len);
        } else if(b >= array_size_offset
                  && b <= array_size_offset + max_onebyte_length) {
            auto array_len = static_cast<size_t>(b - array_size_offset);
            read_array_from(ser, array_len);
        } else if(b > array_size_offset + max_onebyte_length) {
            // First we read the length of the length
            auto len_len = static_cast<size_t>(b - array_size_offset
                                               - max_onebyte_length);
            // Then we read the length bytes
            auto len_buf = cbdc::buffer();
            len_buf.extend(len_len);
            ser.read(len_buf.data(), len_len);
            // Then we turn the read length into a size_t
            auto array_len = deserialize_size(len_buf);
            read_array_from(ser, array_len);
        }
    }

    void rlp_value::read_buffer_from(serializer& ser, size_t size) {
        m_type = rlp_value_type::buffer;
        m_buffer.extend(size);
        ser.read(m_buffer.data(), size);
    }

    void rlp_value::read_array_from(serializer& ser, size_t size) {
        m_type = rlp_value_type::array;
        if(size > 0) {
            auto array_buf = cbdc::buffer();
            array_buf.extend(size);
            ser.read(array_buf.data(), size);
            auto array_deser = cbdc::buffer_serializer(array_buf);
            while(!array_deser.end_of_buffer()) {
                auto v = rlp_value(rlp_value_type::buffer);
                array_deser >> v;
                m_values.push_back(v);
            }
        }
    }

    auto deserialize_size(const cbdc::buffer& buf) -> size_t {
        size_t ret{0};
        auto vec = std::vector<uint8_t>();
        vec.resize(sizeof(size_t));

        std::memcpy(&vec[vec.size() - buf.size()], buf.data(), buf.size());
        std::reverse(vec.begin(), vec.end());
        std::memcpy(&ret, vec.data(), sizeof(size_t));

        return ret;
    }

    auto rlp_decode_access_list(const rlp_value& rlp)
        -> std::optional<parsec::agent::runner::evm_access_list> {
        if(rlp.type() != rlp_value_type::array) {
            return std::nullopt;
        }

        auto access_list = cbdc::parsec::agent::runner::evm_access_list();
        for(size_t i = 0; i < rlp.size(); i++) {
            auto rlp_tuple = rlp.value_at(i);
            auto access_tuple
                = cbdc::parsec::agent::runner::evm_access_tuple();
            if(rlp_tuple.type() == rlp_value_type::array
               && rlp_tuple.size() == 2) {
                access_tuple.m_address
                    = rlp_tuple.value_at(0).value<evmc::address>();
            }
            auto rlp_storage_keys = rlp_tuple.value_at(1);
            for(size_t j = 0; j < rlp_storage_keys.size(); j++) {
                access_tuple.m_storage_keys.push_back(
                    rlp_storage_keys.value_at(j).value<evmc::bytes32>());
            }
            access_list.push_back(access_tuple);
        }

        return access_list;
    }
}
