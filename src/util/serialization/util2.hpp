#ifndef OPENCBDC_TX_SRC_SERIALIZATION_UTIL2_H_
#define OPENCBDC_TX_SRC_SERIALIZATION_UTIL2_H_

#include "hamilton.pb.hpp"
#include "uhs/sentinel/interface.hpp"
#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/transaction.hpp"
#include "util/rpc/format.hpp"
#include "util/rpc/messages.hpp"
#include "util/serialization/buffer_serializer.hpp"

#include <iostream>
#include <optional>
#include <typeinfo>

namespace cbdc {
    /// Serialize object into cbdc::buffer using a cbdc::buffer_serializer.
    /// \tparam T type of objec= to serialize.
    /// \tparam B type of buffer to return, must be cbdc::buffer for this
    ///         template to be enabled.
    /// \return a serialized buffer of the object.
    template<typename T, typename B = buffer>
    auto make_buffer(const T& obj)
        -> std::enable_if_t<std::is_same_v<B, buffer>, cbdc::buffer> {
        auto pkt = cbdc::buffer();

        if(std::is_same<T, cbdc::rpc::response<cbdc::sentinel::response>>::
               value) {
            cbdc::rpc::response<cbdc::sentinel::response> full_resp
                = (cbdc::rpc::response<cbdc::sentinel::response>&)obj;
            cbdc::sentinel::response resp = *(full_resp.m_payload);
            ::transaction::TransactionResponse tx_resp;

            tx_resp.set_message(string_for_response(resp));

            int size = tx_resp.ByteSize();
            void* buff = malloc(size);
            auto ser = cbdc::buffer_serializer(pkt);

            if(buff == NULL) {
                return pkt;
            }

            tx_resp.SerializeToArray(buff, size);
            ser.write(buff, size);
        } else {
            auto sz = serialized_size(obj);
            pkt.extend(sz);
            auto ser = cbdc::buffer_serializer(pkt);
            ser << obj;
        }
        return pkt;
    }

    template<typename T>
    auto from_buffer(cbdc::buffer& buf) -> std::optional<T> {
        T ret{};

        if(std::is_same<T, cbdc::rpc::request<cbdc::transaction::full_tx>>::
               value) {
            ::transaction::Transaction tx_request;
            cbdc::rpc::request<cbdc::transaction::full_tx> ret_not_gen;

            tx_request.ParseFromArray(buf.data(), (int)buf.size());

            std::cout << tx_request.DebugString() << std::endl;
            ret_not_gen = {{}, tx_request.to_full_tx()};

            // Switch to generic object
            auto buff_ser = make_buffer(ret_not_gen);
            auto deser = cbdc::buffer_serializer(buff_ser);

            if(!(deser >> ret)) {
                return std::nullopt;
            }
        } else {
            auto deser = cbdc::buffer_serializer(buf);

            if(!(deser >> ret)) {
                return std::nullopt;
            }
        }
        return ret;
    }
}

#endif // OPENCBDC_TX_SRC_SERIALIZATION_UTIL2_H_