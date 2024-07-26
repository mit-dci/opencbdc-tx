#include "rlp.hpp"

namespace cbdc {

    auto operator<<(serializer& ser, const cbdc::rlp_value& v) -> serializer& {
        v.write_to(ser);
        return ser;
    }

    void rlp_value::write_to(serializer& ser) const {
        if(this->m_type == rlp_value_type::buffer) {
            this->write_buffer_to(ser);
        } else {
            this->write_array_to(ser);
        }
    }

    void rlp_value::write_array_to(serializer& ser) const {
        auto buf = cbdc::buffer();
        auto inner_ser = cbdc::buffer_serializer(buf);
        for(const auto& val : this->m_values) {
            inner_ser << val;
        }
        static constexpr unsigned char array_size_offset = 0xc0;
        serialize_rlp_length(ser, buf.size(), array_size_offset);
        ser.write(buf.data(), buf.size());
    }

    void rlp_value::write_buffer_to(serializer& ser) const {
        static constexpr unsigned char max_single_byte_value = 0x80;

        if(this->m_buffer.size() == 1) {
            std::byte b{};
            std::memcpy(&b, this->m_buffer.data_at(0), 1);
            if(b < std::byte(max_single_byte_value)) {
                ser << b;
                return;
            }
        }
        serialize_rlp_length(ser,
                             this->m_buffer.size(),
                             max_single_byte_value);
        ser.write(this->m_buffer.data(), this->m_buffer.size());
    }

    void
    serialize_rlp_length(serializer& ser, size_t len, unsigned char offset) {
        static constexpr size_t max_onebyte_length = 55;
        if(len <= max_onebyte_length) {
            ser << static_cast<unsigned char>(len + offset);
            return;
        }
        assert(len < (std::numeric_limits<size_t>::max() - offset));
        auto len_ser = serialize_size(len);
        ser << std::byte(static_cast<unsigned char>(
            len_ser.size() + max_onebyte_length + offset));

        for(auto b : len_ser) {
            ser << b;
        }
    }

    auto serialize_size(size_t size) -> std::vector<std::byte> {
        if(size == 0) {
            return {};
        }
        auto buf = make_buffer(size);

        // Trim zeroes
        auto sz = buf.size();
        std::byte b{};
        size_t end_idx = sz;
        auto vec = std::vector<std::byte>();
        while(end_idx > 0) {
            end_idx--;
            std::memcpy(&b, buf.data_at(end_idx), 1);
            if(b != std::byte(0) || !vec.empty()) {
                vec.push_back(b);
            }
        }

        return vec;
    }

    auto rlp_encode_access_list(const parsec::agent::runner::evm_access_list&
                                    access_list) -> rlp_value {
        auto rlp_access_list
            = rlp_value(rlp_value_type::array); // empty by default
        if(!access_list.empty()) {
            for(const auto& access_tuple : access_list) {
                auto storage_keys = rlp_value(rlp_value_type::array);
                for(const auto& storage_key : access_tuple.m_storage_keys) {
                    storage_keys.push_back(make_rlp_value(storage_key));
                }
                rlp_access_list.push_back(
                    make_rlp_array(make_rlp_value(access_tuple.m_address),
                                   storage_keys));
            }
        }
        return rlp_access_list;
    }
}
