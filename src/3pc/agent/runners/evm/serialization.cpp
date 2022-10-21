// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "serialization.hpp"

#include "crypto/sha256.h"
#include "format.hpp"
#include "hash.hpp"
#include "messages.hpp"
#include "rlp.hpp"
#include "util.hpp"
#include "util/common/hash.hpp"
#include "util/serialization/util.hpp"

#include <optional>
#include <secp256k1.h>

namespace cbdc::threepc::agent::runner {
    auto tx_id(const cbdc::threepc::agent::runner::evm_tx& tx,
               uint64_t chain_id) -> cbdc::hash_t {
        auto tx_ser = tx_encode(tx, chain_id);
        return keccak_data(tx_ser.data(), tx_ser.size());
    }

    auto is_valid_rlp_tx(evm_tx_type type, const rlp_value& rlp_tx) -> bool {
        static constexpr size_t elements_in_dynamic_fee_transaction = 12;
        static constexpr size_t elements_in_access_list_transaction = 11;
        static constexpr size_t elements_in_legacy_transaction = 9;

        if(type == evm_tx_type::dynamic_fee
           && rlp_tx.size() != elements_in_dynamic_fee_transaction) {
            return false;
        }
        if(type == evm_tx_type::access_list
           && rlp_tx.size() != elements_in_access_list_transaction) {
            return false;
        }
        if(type == evm_tx_type::legacy
           && rlp_tx.size() != elements_in_legacy_transaction) {
            return false;
        }
        return true;
    }

    auto tx_decode(const cbdc::buffer& buf,
                   const std::shared_ptr<logging::log>& logger,
                   uint64_t chain_id)
        -> std::optional<
            std::shared_ptr<cbdc::threepc::agent::runner::evm_tx>> {
        uint8_t type_byte{};
        std::memcpy(&type_byte, buf.data_at(0), 1);
        size_t rlp_offset = 0;
        auto tx = std::make_shared<cbdc::threepc::agent::runner::evm_tx>();
        if(type_byte >= 1 && type_byte <= 2) {
            tx->m_type = static_cast<evm_tx_type>(type_byte);
            rlp_offset = 1;
        }

        auto buf_cpy = cbdc::buffer();
        buf_cpy.append(buf.data_at(rlp_offset), buf.size() - rlp_offset);
        auto maybe_rlp_tx = cbdc::from_buffer<rlp_value>(buf_cpy);
        if(!maybe_rlp_tx.has_value()) {
            return std::nullopt;
        }
        auto rlp_tx = maybe_rlp_tx.value();

        if(!is_valid_rlp_tx(tx->m_type, rlp_tx)) {
            logger->error("tx is not valid rlp");
            return std::nullopt;
        }

        auto element = size_t{};
        if(tx->m_type == evm_tx_type::dynamic_fee
           || tx->m_type == evm_tx_type::access_list) {
            auto tx_chain_id
                = rlp_tx.value_at(element++).value<evmc::uint256be>();
            if(to_uint64(tx_chain_id) != chain_id) {
                logger->error("tx is wrong chain ID");
                return std::nullopt;
            }
        }

        tx->m_nonce = rlp_tx.value_at(element++).value<evmc::uint256be>();

        if(tx->m_type == evm_tx_type::dynamic_fee) {
            tx->m_gas_tip_cap
                = rlp_tx.value_at(element++).value<evmc::uint256be>();
            tx->m_gas_fee_cap
                = rlp_tx.value_at(element++).value<evmc::uint256be>();
        } else {
            tx->m_gas_price
                = rlp_tx.value_at(element++).value<evmc::uint256be>();
        }
        tx->m_gas_limit = rlp_tx.value_at(element++).value<evmc::uint256be>();
        auto to = rlp_tx.value_at(element++);
        if(to.size() > 0) {
            tx->m_to = to.value<evmc::address>();
        }
        tx->m_value = rlp_tx.value_at(element++).value<evmc::uint256be>();
        auto data = rlp_tx.value_at(element++);
        tx->m_input.resize(data.size());
        std::memcpy(tx->m_input.data(), data.data(), data.size());

        if(tx->m_type == evm_tx_type::dynamic_fee
           || tx->m_type == evm_tx_type::access_list) {
            auto rlp_access_list = rlp_tx.value_at(element++);
            auto access_list = rlp_decode_access_list(rlp_access_list);
            if(access_list.has_value()) {
                tx->m_access_list = access_list.value();
            }
        }

        tx->m_sig.m_v = rlp_tx.value_at(element++).value<evmc::uint256be>();
        if(tx->m_type == evm_tx_type::legacy) {
            auto small_v = to_uint64(tx->m_sig.m_v);
            if(small_v >= eip155_v_offset) {
                auto tx_chain_id = (small_v - eip155_v_offset) / 2;
                if(tx_chain_id != chain_id) {
                    logger->error("tx is wrong chain ID (",
                                  tx_chain_id,
                                  ") where expected (",
                                  chain_id);
                    return std::nullopt;
                }
            }
        }

        tx->m_sig.m_r = rlp_tx.value_at(element++).value<evmc::uint256be>();
        tx->m_sig.m_s = rlp_tx.value_at(element++).value<evmc::uint256be>();
        return tx;
    }

    auto tx_encode(const cbdc::threepc::agent::runner::evm_tx& tx,
                   uint64_t chain_id,
                   bool for_sighash) -> cbdc::buffer {
        auto buf = cbdc::buffer();
        auto ser = cbdc::buffer_serializer(buf);

        auto data_buf = cbdc::buffer();
        data_buf.extend(tx.m_input.size());
        std::memcpy(data_buf.data(), tx.m_input.data(), tx.m_input.size());

        auto to = make_rlp_value(evmc::uint256be(0), true);
        if(tx.m_to.has_value()) {
            to = make_rlp_value(tx.m_to.value(), false);
        }

        auto access_list = rlp_encode_access_list(tx.m_access_list);

        auto rlp_tx = rlp_value(rlp_value_type::array);
        if(tx.m_type == evm_tx_type::dynamic_fee
           || tx.m_type == evm_tx_type::access_list) {
            ser << std::byte(tx.m_type);
            rlp_tx.push_back(make_rlp_value(evmc::uint256be(chain_id), true));
        }

        rlp_tx.push_back(make_rlp_value(tx.m_nonce, true));
        if(tx.m_type == evm_tx_type::dynamic_fee) {
            rlp_tx.push_back(make_rlp_value(tx.m_gas_tip_cap, true));
            rlp_tx.push_back(make_rlp_value(tx.m_gas_fee_cap, true));
        } else {
            rlp_tx.push_back(make_rlp_value(tx.m_gas_price, true));
        }
        rlp_tx.push_back(make_rlp_value(tx.m_gas_limit, true));
        rlp_tx.push_back(to);
        rlp_tx.push_back(make_rlp_value(tx.m_value, true));
        rlp_tx.push_back(rlp_value(data_buf));
        if(tx.m_type == evm_tx_type::dynamic_fee
           || tx.m_type == evm_tx_type::access_list) {
            rlp_tx.push_back(access_list);
        }
        if(for_sighash && tx.m_type == evm_tx_type::legacy) {
            rlp_tx.push_back(make_rlp_value(evmc::uint256be(chain_id), true));
            rlp_tx.push_back(make_rlp_value(uint32_t(0), true));
            rlp_tx.push_back(make_rlp_value(uint32_t(0), true));
        } else if(!for_sighash) {
            rlp_tx.push_back(make_rlp_value(tx.m_sig.m_v, true));
            rlp_tx.push_back(make_rlp_value(tx.m_sig.m_r, true));
            rlp_tx.push_back(make_rlp_value(tx.m_sig.m_s, true));
        }

        ser << rlp_tx;
        return buf;
    }
}
