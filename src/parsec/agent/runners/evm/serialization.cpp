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

namespace cbdc::parsec::agent::runner {
    auto tx_id(const cbdc::parsec::agent::runner::evm_tx& tx,
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

    auto check_tx_decode(
        const cbdc::buffer& buf,
        const std::shared_ptr<logging::log>& logger,
        const std::shared_ptr<cbdc::parsec::agent::runner::evm_tx>& tx)
        -> std::optional<rlp_value> {
        uint8_t type_byte{};
        std::memcpy(&type_byte, buf.data_at(0), 1);
        size_t rlp_offset = 0;
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
            if(logger) {
                logger->error("tx is not valid rlp");
            }
            return std::nullopt;
        }

        return rlp_tx;
    }

    auto tx_decode(const cbdc::buffer& buf,
                   const std::shared_ptr<logging::log>& logger,
                   uint64_t chain_id)
        -> std::optional<
            std::shared_ptr<cbdc::parsec::agent::runner::evm_tx>> {
        auto tx = std::make_shared<cbdc::parsec::agent::runner::evm_tx>();
        auto maybe_rlp_tx = check_tx_decode(buf, logger, tx);
        if(!maybe_rlp_tx.has_value()) {
            return std::nullopt;
        }
        auto rlp_tx = maybe_rlp_tx.value();

        auto element = size_t{};
        if(tx->m_type == evm_tx_type::dynamic_fee
           || tx->m_type == evm_tx_type::access_list) {
            auto tx_chain_id
                = rlp_tx.value_at(element++).value<evmc::uint256be>();
            if(to_uint64(tx_chain_id) != chain_id) {
                if(logger) {
                    logger->error("tx is wrong chain ID");
                }
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
                    if(logger) {
                        logger->error("tx is wrong chain ID (",
                                      tx_chain_id,
                                      ") where expected (",
                                      chain_id);
                    }
                    return std::nullopt;
                }
            }
        }

        tx->m_sig.m_r = rlp_tx.value_at(element++).value<evmc::uint256be>();
        tx->m_sig.m_s = rlp_tx.value_at(element++).value<evmc::uint256be>();
        return tx;
    }

    auto tx_encode(const cbdc::parsec::agent::runner::evm_tx& tx,
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

    auto dryrun_tx_from_json(const Json::Value& json, uint64_t chain_id)
        -> std::optional<
            std::shared_ptr<cbdc::parsec::agent::runner::evm_dryrun_tx>> {
        auto tx = tx_from_json(json, chain_id);
        if(!tx) {
            return std::nullopt;
        }

        auto drtx
            = std::make_shared<cbdc::parsec::agent::runner::evm_dryrun_tx>();
        drtx->m_tx = *tx.value();
        auto maybe_from = address_from_json(json["from"]);
        if(maybe_from) {
            drtx->m_from = maybe_from.value();
        }
        return drtx;
    }

    auto address_from_json(const Json::Value& addr)
        -> std::optional<evmc::address> {
        if(!addr.empty() && addr.isString()) {
            auto addr_str = addr.asString();
            if(addr_str.size() > 2) {
                auto maybe_addr_buf
                    = cbdc::buffer::from_hex(addr_str.substr(2));
                if(maybe_addr_buf) {
                    auto maybe_addr = cbdc::from_buffer<evmc::address>(
                        maybe_addr_buf.value());
                    if(maybe_addr) {
                        return maybe_addr.value();
                    }
                }
            }
        }
        return std::nullopt;
    }

    auto uint256be_from_json(const Json::Value& val)
        -> std::optional<evmc::uint256be> {
        auto maybe_val_buf = buffer_from_json(val);
        if(maybe_val_buf) {
            auto maybe_val
                = cbdc::from_buffer<evmc::uint256be>(maybe_val_buf.value());
            if(maybe_val) {
                return maybe_val.value();
            }
        }
        return std::nullopt;
    }

    auto buffer_from_json(const Json::Value& val)
        -> std::optional<cbdc::buffer> {
        if(!val.empty() && val.isString()) {
            auto val_str = val.asString();
            if(val_str.size() > 2) {
                return cbdc::buffer::from_hex(val_str.substr(2));
            }
        }
        return std::nullopt;
    }

    auto uint256be_or_default(const Json::Value& val, evmc::uint256be def)
        -> evmc::uint256be {
        auto maybe_ui256 = uint256be_from_json(val);
        if(maybe_ui256) {
            return maybe_ui256.value();
        }
        return def;
    }

    auto raw_tx_from_json(const Json::Value& param) -> std::optional<
        std::shared_ptr<cbdc::parsec::agent::runner::evm_tx>> {
        if(!param.isString()) {
            return std::nullopt;
        }
        auto params_str = param.asString();
        auto maybe_raw_tx = cbdc::buffer::from_hex(params_str.substr(2));
        if(!maybe_raw_tx.has_value()) {
            return std::nullopt;
        }

        const std::shared_ptr<logging::log> nolog = nullptr;
        return tx_decode(maybe_raw_tx.value(), nolog);
    }

    auto tx_from_json(const Json::Value& json, uint64_t /*chain_id*/)
        -> std::optional<
            std::shared_ptr<cbdc::parsec::agent::runner::evm_tx>> {
        auto tx = std::make_shared<cbdc::parsec::agent::runner::evm_tx>();
        tx->m_type = evm_tx_type::legacy;
        if(!json["type"].empty() && json["type"].isNumeric()) {
            tx->m_type = static_cast<evm_tx_type>(json["type"].asInt());
        }

        auto maybe_to = address_from_json(json["to"]);
        if(maybe_to) {
            tx->m_to = maybe_to.value();
        }

        tx->m_value = uint256be_or_default(json["value"], evmc::uint256be(0));
        tx->m_nonce = uint256be_or_default(json["nonce"], evmc::uint256be(0));
        tx->m_gas_price
            = uint256be_or_default(json["gasPrice"], evmc::uint256be(0));
        tx->m_gas_limit
            = uint256be_or_default(json["gas"], evmc::uint256be(0));
        tx->m_gas_tip_cap = uint256be_or_default(json["maxPriorityFeePerGas"],
                                                 evmc::uint256be(0));

        tx->m_gas_fee_cap
            = uint256be_or_default(json["maxFeePerGas"], evmc::uint256be(0));

        // TODO
        tx->m_access_list = evm_access_list{};

        auto maybe_input = buffer_from_json(json["data"]);
        if(maybe_input) {
            tx->m_input.resize(maybe_input->size());
            std::memcpy(tx->m_input.data(),
                        maybe_input->data(),
                        maybe_input->size());
        }

        tx->m_sig.m_r = uint256be_or_default(json["r"], evmc::uint256be(0));
        tx->m_sig.m_s = uint256be_or_default(json["s"], evmc::uint256be(0));
        tx->m_sig.m_v = uint256be_or_default(json["v"], evmc::uint256be(0));

        return tx;
    }

    auto tx_to_json(cbdc::parsec::agent::runner::evm_tx& tx,
                    const std::shared_ptr<secp256k1_context>& ctx)
        -> Json::Value {
        auto res = Json::Value();
        res["type"] = to_hex_trimmed(
            evmc::uint256be(static_cast<uint64_t>(tx.m_type)));

        if(tx.m_to.has_value()) {
            res["to"] = "0x" + to_hex(tx.m_to.value());
        }

        res["value"] = to_hex_trimmed(tx.m_value);
        res["nonce"] = to_hex_trimmed(tx.m_nonce);
        res["gasPrice"] = to_hex_trimmed(tx.m_gas_price);
        res["gas"] = to_hex_trimmed(tx.m_gas_limit);

        if(tx.m_type == evm_tx_type::dynamic_fee) {
            res["maxPriorityFeePerGas"] = to_hex_trimmed(tx.m_gas_tip_cap);
            res["maxFeePerGas"] = to_hex_trimmed(tx.m_gas_fee_cap);
        }

        if(!tx.m_input.empty()) {
            auto buf = cbdc::buffer();
            buf.extend(tx.m_input.size());
            std::memcpy(buf.data(), tx.m_input.data(), tx.m_input.size());
            res["input"] = buf.to_hex_prefixed();
        } else {
            res["input"] = "0x";
        }

        if(tx.m_type != evm_tx_type::legacy) {
            res["accessList"] = access_list_to_json(tx.m_access_list);
        }

        res["hash"] = "0x" + cbdc::to_string(tx_id(tx));
        res["r"] = to_hex_trimmed(tx.m_sig.m_r);
        res["s"] = to_hex_trimmed(tx.m_sig.m_s);
        res["v"] = to_hex_trimmed(tx.m_sig.m_v);

        res["chainId"] = to_hex_trimmed(evmc::uint256be(opencbdc_chain_id));

        auto maybe_from_addr = check_signature(tx, ctx);
        if(maybe_from_addr) {
            res["from"] = "0x" + to_hex(maybe_from_addr.value());
        }

        return res;
    }

    auto tx_receipt_to_json(cbdc::parsec::agent::runner::evm_tx_receipt& rcpt,
                            const std::shared_ptr<secp256k1_context>& ctx)
        -> Json::Value {
        auto res = Json::Value();

        auto txid = tx_id(rcpt.m_tx);

        res["transaction"] = tx_to_json(rcpt.m_tx, ctx);
        res["from"] = res["transaction"]["from"];
        res["to"] = res["transaction"]["to"];
        if(rcpt.m_create_address.has_value()) {
            res["contractAddress"]
                = "0x" + to_hex(rcpt.m_create_address.value());
        }
        res["gasUsed"] = to_hex_trimmed(rcpt.m_gas_used);
        res["cumulativeGasUsed"] = to_hex_trimmed(rcpt.m_gas_used);
        res["logs"] = Json::Value(Json::arrayValue);
        res["status"] = rcpt.m_success ? "0x1" : "0x0";

        auto bloom = cbdc::buffer();
        constexpr auto bits_in_32_bytes = 256;
        bloom.extend(bits_in_32_bytes);
        for(auto& l : rcpt.m_logs) {
            res["logs"].append(tx_log_to_json(l, rcpt.m_ticket_number, txid));
            add_to_bloom(bloom, cbdc::make_buffer(l.m_addr));
            for(auto& t : l.m_topics) {
                add_to_bloom(bloom, cbdc::make_buffer(t));
            }
        }

        res["logsBloom"] = bloom.to_hex_prefixed();

        if(!rcpt.m_output_data.empty()) {
            auto buf = cbdc::buffer();
            buf.extend(rcpt.m_output_data.size());
            std::memcpy(buf.data(),
                        rcpt.m_output_data.data(),
                        rcpt.m_output_data.size());
            res["output_data"] = buf.to_hex_prefixed();
        }

        res["success"] = "0x1";
        res["transactionIndex"] = "0x0";
        res["transactionHash"] = "0x" + to_string(txid);
        auto tn256 = evmc::uint256be(rcpt.m_ticket_number);
        res["blockHash"] = "0x" + to_hex(tn256);
        res["blockNumber"] = to_hex_trimmed(tn256);
        return res;
    }

    auto tx_log_to_json(cbdc::parsec::agent::runner::evm_log& log,
                        interface::ticket_number_type tn,
                        cbdc::hash_t txid) -> Json::Value {
        auto res = Json::Value();
        res["address"] = "0x" + to_hex(log.m_addr);
        if(!log.m_data.empty()) {
            auto buf = cbdc::buffer();
            buf.extend(log.m_data.size());
            std::memcpy(buf.data(), log.m_data.data(), log.m_data.size());
            res["data"] = buf.to_hex_prefixed();
        } else {
            res["data"] = "0x";
        }
        res["topics"] = Json::Value(Json::arrayValue);
        for(auto& t : log.m_topics) {
            res["topics"].append("0x" + to_hex(t));
        }

        auto tn256 = evmc::uint256be(tn);
        res["blockHash"] = "0x" + to_hex(tn256);
        res["blockNumber"] = to_hex_trimmed(tn256);

        res["transactionIndex"] = "0x0";
        res["transactionHash"] = "0x" + cbdc::to_string(txid);
        res["logIndex"] = "0x0";
        return res;
    }

    auto access_list_to_json(cbdc::parsec::agent::runner::evm_access_list& al)
        -> Json::Value {
        auto res = Json::Value(Json::arrayValue);
        for(auto& tuple : al) {
            auto json_tuple = Json::Value();
            json_tuple["address"] = to_hex(tuple.m_address);
            json_tuple["storageKeys"] = Json::Value(Json::arrayValue);
            for(auto& k : tuple.m_storage_keys) {
                json_tuple["storageKeys"].append(to_hex(k));
            }
            res.append(json_tuple);
        }
        return res;
    }
}
