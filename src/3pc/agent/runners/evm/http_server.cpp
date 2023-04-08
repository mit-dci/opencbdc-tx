// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "http_server.hpp"

#include "3pc/agent/runners/evm/format.hpp"
#include "3pc/agent/runners/evm/hash.hpp"
#include "3pc/agent/runners/evm/impl.hpp"
#include "3pc/agent/runners/evm/math.hpp"
#include "3pc/agent/runners/evm/serialization.hpp"
#include "3pc/agent/runners/evm/util.hpp"
#include "impl.hpp"
#include "util/common/hash.hpp"
#include "util/serialization/format.hpp"

#include <future>

using namespace cbdc::threepc::agent::runner;

namespace cbdc::threepc::agent::rpc {
    http_server::http_server(std::unique_ptr<server_type> srv,
                             std::shared_ptr<broker::interface> broker,
                             std::shared_ptr<logging::log> log,
                             const cbdc::threepc::config& cfg)
        : server_interface(std::move(broker), std::move(log), cfg),
          m_srv(std::move(srv)) {
        m_srv->register_handler_callback(
            [&](const std::string& method,
                const Json::Value& params,
                const server_type::result_callback_type& callback) {
                return request_handler(method, params, callback);
            });
    }

    http_server::~http_server() {
        m_log->trace("Agent server shutting down...");
        m_srv.reset();
        m_log->trace("Shut down agent server");
    }

    auto http_server::init() -> bool {
        return m_srv->init();
    }

    auto http_server::request_handler(
        const std::string& method,
        const Json::Value& params,
        const server_type::result_callback_type& callback) -> bool {
        m_log->trace("http_server::request_handler() received request",
                     method);

        auto maybe_handled = handle_supported(method, params, callback);
        if(maybe_handled.has_value()) {
            return maybe_handled.value();
        }

        maybe_handled = handle_static(method, params, callback);
        if(maybe_handled.has_value()) {
            return maybe_handled.value();
        }

        return handle_unsupported(method, params, callback);
    }

    auto http_server::handle_supported(
        const std::string& method,
        const Json::Value& params,
        const server_type::result_callback_type& callback)
        -> std::optional<bool> {
        if(method == "eth_sendRawTransaction") {
            return handle_send_raw_transaction(params, callback);
        }

        if(method == "eth_sendTransaction") {
            return handle_send_transaction(params, callback);
        }

        if(method == "eth_getTransactionCount") {
            return handle_get_transaction_count(params, callback);
        }

        if(method == "eth_call") {
            return handle_call(params, callback);
        }

        if(method == "eth_estimateGas") {
            return handle_estimate_gas(params, callback);
        }

        if(method == "eth_gasPrice") {
            return handle_gas_price(params, callback);
        }

        if(method == "eth_getCode") {
            return handle_get_code(params, callback);
        }

        if(method == "eth_getBalance") {
            return handle_get_balance(params, callback);
        }

        if(method == "eth_accounts") {
            return handle_accounts(params, callback);
        }

        if(method == "eth_getTransactionByHash") {
            return handle_get_transaction_by_hash(params, callback);
        }

        if(method == "eth_getTransactionReceipt") {
            return handle_get_transaction_receipt(params, callback);
        }

        if(method == "eth_getBlockByNumber"
           || method == "eth_getBlockByHash") {
            return handle_get_block(params, callback);
        }

        if(method == "eth_getBlockTransactionCountByHash"
           || method == "eth_getBlockTransactionCountByNumber") {
            return handle_get_block_txcount(params, callback);
        }

        if(method == "eth_getTransactionByBlockHashAndIndex"
           || method == "eth_getTransactionByBlockNumberAndIndex") {
            return handle_get_block_tx(params, callback);
        }

        if(method == "eth_blockNumber") {
            return handle_block_number(params, callback);
        }

        if(method == "eth_feeHistory") {
            return handle_fee_history(params, callback);
        }

        if(method == "eth_getLogs") {
            return handle_get_logs(params, callback);
        }

        if(method == "eth_getStorageAt") {
            return handle_get_storage_at(params, callback);
        }

        return std::nullopt;
    }

    auto http_server::handle_static(
        const std::string& method,
        const Json::Value& params,
        const server_type::result_callback_type& callback)
        -> std::optional<bool> {
        if(method == "eth_chainId" || method == "net_version") {
            return handle_chain_id(params, callback);
        }

        if(method == "web3_clientVersion") {
            return handle_client_version(params, callback);
        }

        if(method == "eth_decodeRawTransaction") {
            return handle_decode_raw_transaction(params, callback);
        }

        if(method == "web3_sha3") {
            return handle_sha3(params, callback);
        }

        return std::nullopt;
    }

    auto http_server::handle_unsupported(
        const std::string& method,
        const Json::Value& params,
        const server_type::result_callback_type& callback) -> bool {
        if(method == "eth_signTransaction" || method == "eth_sign") {
            return handle_error(
                params,
                callback,
                error_code::wallet_not_supported,
                "Wallet support not enabled - sign transactions "
                "locally before submitting");
        }

        if(method == "eth_uninstallFilter"
           || method == "eth_newPendingTransactionFilter"
           || method == "eth_newFilter" || method == "eth_newBlockFilter"
           || method == "eth_getFilterLogs"
           || method == "eth_getFilterChanges") {
            return handle_error(params,
                                callback,
                                error_code::wallet_not_supported,
                                "OpenCBDC does not support filters");
        }

        if(method == "eth_getWork" || method == "eth_submitWork"
           || method == "eth_submitHashrate") {
            return handle_error(params,
                                callback,
                                error_code::mining_not_supported,
                                "OpenCBDC does not use mining");
        }

        if(method == "evm_increaseTime") {
            return handle_error(params,
                                callback,
                                error_code::time_travel_not_supported,
                                "OpenCBDC does not support time travel");
        }

        if(method == "eth_getCompilers" || method == "eth_compileSolidity"
           || method == "eth_compileLLL" || method == "eth_compileSerpent") {
            return handle_error(params,
                                callback,
                                error_code::compiler_not_supported,
                                "OpenCBDC does not provide compiler support - "
                                "compile contracts locally before submitting");
        }

        if(method == "eth_coinbase") {
            return handle_error(params,
                                callback,
                                error_code::coinbase_not_supported,
                                "Coinbase payouts are not used in OpenCBDC");
        }

        if(method == "eth_getUncleByBlockHashAndIndex"
           || method == "eth_getUncleByBlockNumberAndIndex") {
            // There are no uncle blocks in OpenCBDC ever
            return handle_error(params,
                                callback,
                                error_code::uncles_not_supported,
                                "Uncle block not found");
        }

        if(method == "eth_getUncleCountByBlockHash"
           || method == "eth_getUncleCountByBlockNumber"
           || method == "eth_hashrate") {
            // There are no uncle blocks in OpenCBDC ever
            return handle_number(params, callback, 0);
        }

        if(method == "eth_mining") {
            return handle_boolean(params, callback, false);
        }

        if(method == "eth_syncing") {
            return handle_boolean(params, callback, false);
        }

        if(method == "net_listening") {
            return handle_boolean(params, callback, false);
        }

        if(method == "net_peerCount") {
            return handle_number(params, callback, 1);
        }

        m_log->warn("Unknown method", method);
        return handle_error(params,
                            callback,
                            error_code::unknown_method,
                            "Unknown method: " + method);
    }

    auto http_server::handle_decode_raw_transaction(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        auto maybe_tx = raw_tx_from_json(params[0]);
        if(!maybe_tx.has_value()) {
            m_log->warn("Unable to deserialize transaction");
            return false;
        }
        auto& tx = maybe_tx.value();
        auto ret = Json::Value();
        ret["result"] = tx_to_json(*tx, m_secp);
        callback(ret);
        return true;
    }

    auto http_server::handle_send_raw_transaction(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        auto res_cb = std::function<void(interface::exec_return_type)>();

        auto maybe_tx = raw_tx_from_json(params[0]);
        if(!maybe_tx.has_value()) {
            m_log->warn("Unable to deserialize transaction");
            return false;
        }
        auto& tx = maybe_tx.value();
        auto runner_params = make_buffer(*tx);

        return exec_tx(callback,
                       runner::evm_runner_function::execute_transaction,
                       runner_params,
                       false,
                       [callback, tx](const interface::exec_return_type&) {
                           auto txid = cbdc::make_buffer(tx_id(*tx));
                           auto ret = Json::Value();
                           ret["result"] = "0x" + txid.to_hex();
                           callback(ret);
                       });
    }

    auto http_server::handle_fee_history(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        auto ret = Json::Value();

        if(!params.isArray() || params.size() < 3 || !params[0].isString()
           || !params[1].isString() || !params[2].isArray()) {
            m_log->warn("Invalid parameters to feeHistory");
            return false;
        }

        auto blocks_str = params[0].asString();
        uint64_t blocks = 0;
        auto end_block_str = params[1].asString();
        uint64_t end_block = 0;
        if(end_block_str == "latest" || end_block_str == "pending") {
            end_block = m_broker->highest_ticket();
        } else {
            auto maybe_block = uint256be_from_json(params[1]);
            if(!maybe_block) {
                ret["error"] = Json::Value();
                ret["error"]["code"] = error_code::invalid_block_identifier;
                ret["error"]["message"] = "Invalid block identifier";
                callback(ret);
                return true;
            }
            end_block = to_uint64(maybe_block.value());
        }
        blocks = std::stoull(blocks_str);
        if(blocks > end_block) {
            blocks = end_block;
        }
        ret["result"] = Json::Value();
        ret["result"]["oldestBlock"]
            = to_hex_trimmed(evmc::uint256be(end_block - blocks));
        ret["result"]["reward"] = Json::Value(Json::arrayValue);
        ret["result"]["baseFeePerGas"] = Json::Value(Json::arrayValue);
        ret["result"]["gasUsedRatio"] = Json::Value(Json::arrayValue);
        for(uint64_t i = 0; i < blocks; i++) {
            auto rwd = Json::Value(Json::arrayValue);
            for(Json::ArrayIndex j = 0; j < params[2].size(); j++) {
                rwd.append("0x0");
            }
            ret["result"]["reward"].append(rwd);
            ret["result"]["baseFeePerGas"].append("0x0");
            ret["result"]["gasUsedRatio"].append(0.0);
        }
        ret["result"]["baseFeePerGas"].append("0x0");
        callback(ret);
        return true;
    }

    auto http_server::handle_get_transaction_count(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        if(!params.isArray() || params.empty() || !params[0].isString()) {
            m_log->warn("Invalid parameters to getTransactionCount");
            return false;
        }

        auto params_str = params[0].asString();
        auto maybe_runner_params
            = cbdc::buffer::from_hex(params_str.substr(2));
        if(!maybe_runner_params.has_value()) {
            m_log->warn("Unable to decode params", params_str);
            return false;
        }
        auto runner_params = std::move(maybe_runner_params.value());
        return exec_tx(
            callback,
            runner::evm_runner_function::read_account,
            runner_params,
            true,
            [callback, runner_params](interface::exec_return_type res) {
                auto ret = Json::Value();

                auto& updates = std::get<return_type>(res);
                auto it = updates.find(runner_params);

                if(it == updates.end()) {
                    // For accounts that don't exist yet, return 1
                    ret["result"] = to_hex_trimmed(evmc::uint256be(1));
                    callback(ret);
                }

                auto maybe_acc
                    = cbdc::from_buffer<runner::evm_account>(it->second);
                if(!maybe_acc.has_value()) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::internal_error;
                    ret["error"]["message"] = "Internal error";
                    callback(ret);
                    return;
                }

                auto& acc = maybe_acc.value();

                auto tx_count = acc.m_nonce + evmc::uint256be(1);
                ret["result"] = to_hex_trimmed(tx_count);
                callback(ret);
            });
    }

    auto http_server::handle_get_balance(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        if(!params.isArray() || params.empty() || !params[0].isString()) {
            m_log->warn("Invalid parameters to getBalance");
            return false;
        }

        auto params_str = params[0].asString();
        auto maybe_runner_params
            = cbdc::buffer::from_hex(params_str.substr(2));
        if(!maybe_runner_params.has_value()) {
            m_log->warn("Unable to decode params", params_str);
            return false;
        }
        auto runner_params = std::move(maybe_runner_params.value());
        return exec_tx(
            callback,
            runner::evm_runner_function::read_account,
            runner_params,
            true,
            [callback, runner_params](interface::exec_return_type res) {
                auto ret = Json::Value();
                auto& updates = std::get<return_type>(res);
                auto it = updates.find(runner_params);
                if(it == updates.end() || it->second.size() == 0) {
                    // Return 0 for non-existent accounts
                    ret["result"] = "0x0";
                    callback(ret);
                    return;
                }

                auto maybe_acc
                    = cbdc::from_buffer<runner::evm_account>(it->second);
                if(!maybe_acc.has_value()) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::internal_error;
                    ret["error"]["message"] = "Internal error";
                    callback(ret);
                    return;
                }

                auto& acc = maybe_acc.value();
                ret["result"] = to_hex_trimmed(acc.m_balance);
                callback(ret);
            });
    }

    auto http_server::handle_get_storage_at(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        if(!params.isArray() || params.empty() || !params[0].isString()
           || !params[1].isString()) {
            m_log->warn("Invalid parameters to getBalance");
            return false;
        }

        auto maybe_addr = address_from_json(params[0]);
        if(!maybe_addr.has_value()) {
            m_log->warn("Unable to decode params");
            return false;
        }
        auto key_str = params[1].asString();
        auto maybe_key = from_hex<evmc::bytes32>(key_str);
        if(!maybe_key.has_value()) {
            m_log->warn("Unable to decode params", key_str);
            return false;
        }

        auto runner_params = cbdc::make_buffer(
            storage_key{maybe_addr.value(), maybe_key.value()});
        return exec_tx(
            callback,
            runner::evm_runner_function::read_account_storage,
            runner_params,
            true,
            [callback, runner_params](interface::exec_return_type res) {
                auto ret = Json::Value();
                auto& updates = std::get<return_type>(res);
                auto it = updates.find(runner_params);
                if(it == updates.end() || it->second.size() == 0) {
                    // Return empty for non-existent data
                    ret["result"] = "0x";
                    callback(ret);
                    return;
                }

                ret["result"] = "0x" + it->second.to_hex();
                callback(ret);
            });
    }

    auto http_server::handle_get_transaction_by_hash(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        if(!params.isArray() || params.empty() || !params[0].isString()) {
            m_log->warn("Invalid parameters to getTransactionByHash");
            return false;
        }
        auto params_str = params[0].asString();
        auto maybe_runner_params
            = cbdc::buffer::from_hex(params_str.substr(2));
        if(!maybe_runner_params.has_value()) {
            m_log->warn("Unable to decode params", params_str);
            return false;
        }
        auto runner_params = std::move(maybe_runner_params.value());
        return exec_tx(
            callback,
            runner::evm_runner_function::get_transaction_receipt,
            runner_params,
            true,
            [callback, runner_params, this](interface::exec_return_type res) {
                auto ret = Json::Value();
                auto& updates = std::get<return_type>(res);
                auto it = updates.find(runner_params);
                if(it == updates.end() || it->second.size() == 0) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::not_found;
                    ret["error"]["message"] = "Transaction not found";
                    callback(ret);
                    return;
                }

                auto maybe_tx
                    = cbdc::from_buffer<runner::evm_tx_receipt>(it->second);
                if(!maybe_tx.has_value()) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::internal_error;
                    ret["error"]["message"] = "Internal error";
                    callback(ret);
                    return;
                }

                auto& tx_rcpt = maybe_tx.value();
                auto json_tx = tx_to_json(tx_rcpt.m_tx, m_secp);

                // Append block data
                auto block_num = evmc::uint256be(tx_rcpt.m_ticket_number);
                json_tx["blockHash"] = "0x" + to_hex(block_num);
                json_tx["blockNumber"] = to_hex_trimmed(block_num);
                json_tx["transactionIndex"] = "0x0";

                ret["result"] = json_tx;
                callback(ret);
            });
    }

    auto http_server::extract_evm_log_query_addresses(
        Json::Value params,
        const server_type::result_callback_type& callback,
        evm_log_query& qry) -> bool {
        auto parseError = false;
        if(params[0]["address"].isString()) {
            auto maybe_addr = address_from_json(params[0]["address"]);
            if(maybe_addr) {
                qry.m_addresses.push_back(maybe_addr.value());
            } else {
                parseError = true;
            }
        } else if(params[0]["address"].isArray()) {
            for(auto& val : params[0]["address"]) {
                auto maybe_addr = address_from_json(val);
                if(maybe_addr) {
                    qry.m_addresses.push_back(maybe_addr.value());
                } else {
                    parseError = true;
                }
            }
        }

        if(qry.m_addresses.empty() || parseError) {
            auto ret = Json::Value();
            ret["error"] = Json::Value();
            ret["error"]["code"] = error_code::invalid_address;
            ret["error"]["message"]
                = "Address(es) in your query are either absent or invalid";
            callback(ret);
            return false;
        }

        return true;
    }

    auto http_server::extract_evm_log_query_topics(
        Json::Value params,
        const server_type::result_callback_type& callback,
        evm_log_query& qry) -> bool {
        auto parseError = false;
        if(params[0]["topics"].isArray()) {
            for(auto& val : params[0]["topics"]) {
                auto maybe_topic = from_hex<evmc::bytes32>(val.asString());
                if(maybe_topic) {
                    qry.m_topics.push_back(maybe_topic.value());
                } else {
                    parseError = true;
                }
            }
        }

        if(qry.m_topics.empty() || parseError) {
            auto ret = Json::Value();
            ret["error"] = Json::Value();
            ret["error"]["code"] = error_code::invalid_topic;
            ret["error"]["message"]
                = "Topic(s) in your query are either absent or invalid";
            callback(ret);
            return false;
        }

        return true;
    }

    auto http_server::extract_evm_log_query_block(
        Json::Value params,
        const server_type::result_callback_type& callback,
        evm_log_query& qry) -> bool {
        auto ret = Json::Value();

        if(params[0]["blockhash"].isString()) {
            auto maybe_block_num
                = uint256be_from_hex(params[0]["blockhash"].asString());
            if(!maybe_block_num) {
                m_log->warn("Invalid blockNumber / hash parameter");
                return false;
            }
            qry.m_from_block = to_uint64(maybe_block_num.value());
            qry.m_to_block = qry.m_from_block;
        } else if(params[0]["fromBlock"].isString()
                  && params[0]["toBlock"].isString()) {
            auto highest_ticket_number = m_broker->highest_ticket();
            if(params[0]["fromBlock"].asString() == "latest") {
                qry.m_from_block = highest_ticket_number;
            } else {
                auto maybe_block_num
                    = uint256be_from_hex(params[0]["fromBlock"].asString());
                if(!maybe_block_num) {
                    m_log->warn("Invalid fromBlock parameter");
                    return false;
                }
                qry.m_from_block = to_uint64(maybe_block_num.value());
            }

            if(params[0]["toBlock"].asString() == "latest") {
                qry.m_to_block = highest_ticket_number;
            } else {
                auto maybe_block_num
                    = uint256be_from_hex(params[0]["toBlock"].asString());
                if(!maybe_block_num) {
                    m_log->warn("Invalid toBlock parameter");
                    return false;
                }
                qry.m_to_block = to_uint64(maybe_block_num.value());
            }
        } else {
            ret["error"] = Json::Value();
            ret["error"]["code"] = error_code::invalid_block_parameter;
            ret["error"]["message"]
                = "from/toBlock or blockHash parameter missing";
            callback(ret);
            return false;
        }

        auto block_count = static_cast<int64_t>(qry.m_to_block)
                         - static_cast<int64_t>(qry.m_from_block);

        if(block_count < 0) {
            ret["error"] = Json::Value();
            ret["error"]["code"] = error_code::from_block_after_to;
            ret["error"]["message"] = "From block cannot be after to block";
            callback(ret);
            return false;
        }

        auto uint_block_count = static_cast<uint64_t>(block_count);

        constexpr auto max_block_count = 100;
        if(uint_block_count * qry.m_addresses.size() > max_block_count) {
            ret["error"] = Json::Value();
            ret["error"]["code"] = error_code::block_range_too_large;
            ret["error"]["message"] = "The product of address count and block "
                                      "range in your query cannot exceed 100";
            callback(ret);
            return false;
        }

        return true;
    }

    auto http_server::parse_evm_log_query(
        const Json::Value& params,
        const server_type::result_callback_type& callback)
        -> std::optional<threepc::agent::runner::evm_log_query> {
        evm_log_query qry;

        auto success = extract_evm_log_query_addresses(params, callback, qry);
        if(!success) {
            return std::nullopt;
        }

        success = extract_evm_log_query_topics(params, callback, qry);
        if(!success) {
            return std::nullopt;
        }

        success = extract_evm_log_query_block(params, callback, qry);
        if(!success) {
            return std::nullopt;
        }

        return qry;
    }

    void http_server::handle_get_logs_result(
        const server_type::result_callback_type& callback,
        const cbdc::buffer& runner_params,
        const runner::evm_log_query& qry,
        interface::exec_return_type res) {
        auto ret = Json::Value();
        auto& updates = std::get<return_type>(res);
        auto it = updates.find(runner_params);
        if(it == updates.end() || it->second.size() == 0) {
            ret["error"] = Json::Value();
            ret["error"]["code"] = error_code::not_found;
            ret["error"]["message"] = "Logs not found";
            callback(ret);
            return;
        }

        auto maybe_logs
            = cbdc::from_buffer<std::vector<evm_log_index>>(it->second);
        if(!maybe_logs.has_value()) {
            ret["error"] = Json::Value();
            ret["error"]["code"] = error_code::internal_error;
            ret["error"]["message"] = "Internal error";
            callback(ret);
            return;
        }

        auto& logs = maybe_logs.value();
        ret["result"] = Json::Value(Json::arrayValue);
        for(auto& log_idx : logs) {
            for(auto& log : log_idx.m_logs) {
                auto match = false;
                for(auto& have_topic : log.m_topics) {
                    for(const auto& want_topic : qry.m_topics) {
                        if(have_topic == want_topic) {
                            match = true;
                            break;
                        }
                    }
                    if(match) {
                        break;
                    }
                }
                if(match) {
                    ret["result"].append(
                        tx_log_to_json(log,
                                       log_idx.m_ticket_number,
                                       log_idx.m_txid));
                }
            }
        }
        callback(ret);
    }

    auto http_server::handle_get_logs(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        if(!params.isArray() || params.empty() || !params[0].isObject()) {
            m_log->warn("Invalid parameters to getLogs");
            return false;
        }

        auto maybe_qry = parse_evm_log_query(params, callback);
        if(!maybe_qry) {
            // parse_evm_log_query has already reported the error back to the
            // client
            return true;
        }
        auto qry = maybe_qry.value();
        auto runner_params = cbdc::make_buffer(qry);
        return exec_tx(
            callback,
            runner::evm_runner_function::get_logs,
            runner_params,
            true,
            [callback, runner_params, qry](interface::exec_return_type res) {
                handle_get_logs_result(callback,
                                       runner_params,
                                       qry,
                                       std::move(res));
            });
    }

    auto http_server::handle_get_transaction_receipt(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        if(!params.isArray() || params.empty() || !params[0].isString()) {
            m_log->warn("Invalid parameters to getTransactionReceipt");
            return false;
        }
        auto params_str = params[0].asString();
        auto maybe_runner_params
            = cbdc::buffer::from_hex(params_str.substr(2));
        if(!maybe_runner_params.has_value()) {
            m_log->warn("Unable to decode params", params_str);
            return false;
        }
        auto runner_params = std::move(maybe_runner_params.value());
        return exec_tx(
            callback,
            runner::evm_runner_function::get_transaction_receipt,
            runner_params,
            true,
            [callback, runner_params, this](interface::exec_return_type res) {
                auto ret = Json::Value();
                auto& updates = std::get<return_type>(res);
                auto it = updates.find(runner_params);
                if(it == updates.end() || it->second.size() == 0) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::not_found;
                    ret["error"]["message"] = "Transaction not found";
                    callback(ret);
                    return;
                }

                auto maybe_rcpt
                    = cbdc::from_buffer<runner::evm_tx_receipt>(it->second);
                if(!maybe_rcpt.has_value()) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::internal_error;
                    ret["error"]["message"] = "Internal error";
                    callback(ret);
                    return;
                }

                auto& rcpt = maybe_rcpt.value();

                ret["result"] = tx_receipt_to_json(rcpt, m_secp);
                callback(ret);
            });
    }

    auto http_server::handle_get_code(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        if(!params.isArray() || params.empty() || !params[0].isString()) {
            m_log->warn("Invalid parameters to getBalance");
            return false;
        }

        auto params_str = params[0].asString();
        auto maybe_runner_params
            = cbdc::buffer::from_hex(params_str.substr(2));
        if(!maybe_runner_params.has_value()) {
            m_log->warn("Unable to decode params", params_str);
            return false;
        }
        auto runner_params = std::move(maybe_runner_params.value());
        return exec_tx(
            callback,
            runner::evm_runner_function::read_account_code,
            runner_params,
            true,
            [callback, runner_params](interface::exec_return_type res) {
                auto ret = Json::Value();
                auto& updates = std::get<return_type>(res);
                auto it = updates.find(runner_params);
                if(it == updates.end() || it->second.size() == 0) {
                    // Return empty buffer when code not found
                    ret["result"] = "0x";
                    callback(ret);
                    return;
                }
                ret["result"] = "0x" + it->second.to_hex();
                callback(ret);
            });
    }

    auto http_server::handle_chain_id(
        const Json::Value& /*params*/,
        const server_type::result_callback_type& callback) -> bool {
        auto ret = Json::Value();
        ret["result"] = to_hex_trimmed(evmc::uint256be(opencbdc_chain_id));
        callback(ret);
        return true;
    }

    auto http_server::handle_block_number(
        const Json::Value& /*params*/,
        const server_type::result_callback_type& callback) -> bool {
        auto highest_ticket_number = m_broker->highest_ticket();
        auto ret = Json::Value();
        ret["result"] = to_hex_trimmed(evmc::uint256be(highest_ticket_number));
        callback(ret);
        return true;
    }

    auto http_server::fetch_block(
        Json::Value params,
        const server_type::result_callback_type& callback,
        const std::function<void(interface::exec_return_type, cbdc::buffer)>&
            res_cb) -> bool {
        if(!params.isArray() || params.empty() || !params[0].isString()
           || (params.size() > 1 && !params[1].isBool())) {
            m_log->warn("Invalid parameters to getBlock", params.size());
            return false;
        }

        cbdc::buffer runner_params;
        if(params[0].asString() == "latest") {
            runner_params = cbdc::make_buffer(
                evmc::uint256be(m_broker->highest_ticket()));
        } else {
            auto maybe_block_num = uint256be_from_hex(params[0].asString());
            if(!maybe_block_num) {
                m_log->warn("Invalid blockNumber / hash parameter");
                return false;
            }
            runner_params = cbdc::make_buffer(maybe_block_num.value());
        }

        return exec_tx(
            callback,
            runner::evm_runner_function::get_block,
            runner_params,
            true,
            [res_cb, runner_params](interface::exec_return_type res) {
                res_cb(std::move(res), runner_params);
            });
    }

    auto http_server::handle_get_block(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        auto include_tx_details = params[1].asBool();
        return fetch_block(
            params,
            callback,
            [this, callback, include_tx_details](
                interface::exec_return_type res,
                const cbdc::buffer& runner_params) {
                auto& updates = std::get<return_type>(res);
                auto it = updates.find(runner_params);
                auto ret = Json::Value();
                if(it == updates.end() || it->second.size() == 0) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::not_found;
                    ret["error"]["message"] = "Data was not found";
                    callback(ret);
                    return;
                }

                auto maybe_pretend_block
                    = cbdc::from_buffer<evm_pretend_block>(it->second);
                if(!maybe_pretend_block) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::internal_error;
                    ret["error"]["message"] = "Internal error";
                    callback(ret);
                    return;
                }
                ret["result"] = Json::Value();
                auto blk = maybe_pretend_block.value();
                auto tn256 = evmc::uint256be(blk.m_ticket_number);
                ret["result"]["number"] = to_hex_trimmed(tn256);
                ret["result"]["hash"] = "0x" + to_hex(tn256);
                ret["result"]["parentHash"]
                    = "0x" + to_hex(evmc::uint256be(blk.m_ticket_number - 1));
                ret["result"]["gasLimit"] = "0xffffffff";
                ret["result"]["gasUsed"] = "0x0";
                ret["result"]["baseFeePerGas"] = "0x0";
                ret["result"]["miner"]
                    = "0x0000000000000000000000000000000000000000";
                ret["result"]["transactions"] = Json::Value(Json::arrayValue);
                ret["result"]["nonce"] = "0x0000000000000000";

                auto bloom = cbdc::buffer();
                constexpr auto bits_in_32_bytes = 256;
                bloom.extend(bits_in_32_bytes);
                uint64_t timestamp = 0;
                for(auto& tx_rcpt : blk.m_transactions) {
                    if(tx_rcpt.m_timestamp > timestamp) {
                        timestamp = tx_rcpt.m_timestamp;
                    }
                    for(auto& l : tx_rcpt.m_logs) {
                        add_to_bloom(bloom, cbdc::make_buffer(l.m_addr));
                        for(auto& t : l.m_topics) {
                            add_to_bloom(bloom, cbdc::make_buffer(t));
                        }
                    }
                    if(include_tx_details) {
                        auto json_tx = tx_to_json(tx_rcpt.m_tx, m_secp);
                        json_tx["blockHash"] = "0x" + to_hex(tn256);
                        json_tx["blockNumber"] = to_hex_trimmed(tn256);
                        json_tx["transactionIndex"] = "0x0";
                        ret["result"]["transactions"].append(json_tx);
                    } else {
                        ret["result"]["transactions"].append(
                            "0x" + to_string(tx_id(tx_rcpt.m_tx)));
                    }
                }
                ret["result"]["timestamp"]
                    = to_hex_trimmed(evmc::uint256be(timestamp));
                ret["result"]["extraData"] = "0x" + to_hex(evmc::uint256be(0));
                ret["result"]["logsBloom"] = bloom.to_hex_prefixed();
                // We don't have any uncles ever
                ret["result"]["uncles"] = Json::Value(Json::arrayValue);
                callback(ret);
            });
    }

    auto http_server::handle_get_block_txcount(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        return fetch_block(
            std::move(params),
            callback,
            [callback](interface::exec_return_type res,
                       const cbdc::buffer& runner_params) {
                auto& updates = std::get<return_type>(res);
                auto it = updates.find(runner_params);
                auto ret = Json::Value();
                if(it == updates.end() || it->second.size() == 0) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::not_found;
                    ret["error"]["message"] = "Data was not found";
                    callback(ret);
                    return;
                }

                auto maybe_pretend_block
                    = cbdc::from_buffer<evm_pretend_block>(it->second);
                if(!maybe_pretend_block) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::internal_error;
                    ret["error"]["message"] = "Internal error";
                    callback(ret);
                    return;
                }
                auto blk = maybe_pretend_block.value();
                ret["result"] = to_hex_trimmed(
                    evmc::uint256be(blk.m_transactions.size()));
                callback(ret);
            });
    }

    auto
    http_server::handle_sha3(Json::Value params,
                             const server_type::result_callback_type& callback)
        -> bool {
        if(!params.isArray() || params.empty() || !params[0].isString()) {
            m_log->warn("Invalid parameters to sha3");
            return false;
        }

        auto maybe_buf = buffer_from_json(params[0]);
        if(!maybe_buf) {
            m_log->warn("Could not parse argument as buffer");
            return false;
        }

        auto input = maybe_buf.value();
        auto sha3 = keccak_data(input.data(), input.size());

        auto ret = Json::Value();
        ret["result"] = "0x" + cbdc::to_string(sha3);
        callback(ret);
        return true;
    }

    auto http_server::handle_error(
        const Json::Value& /*params*/,
        const server_type::result_callback_type& callback,
        int code,
        const std::string& message) -> bool {
        auto ret = Json::Value();
        ret["error"] = Json::Value();
        ret["error"]["code"] = code;
        ret["error"]["message"] = message;
        callback(ret);
        return true;
    }

    auto http_server::handle_number(
        const Json::Value& /*params*/,
        const server_type::result_callback_type& callback,
        uint64_t number) -> bool {
        auto ret = Json::Value();
        ret["result"] = to_hex_trimmed(evmc::uint256be(number));
        callback(ret);
        return true;
    }

    auto http_server::handle_boolean(
        const Json::Value& /*params*/,
        const server_type::result_callback_type& callback,
        bool result) -> bool {
        auto ret = Json::Value();
        ret["result"] = result;
        callback(ret);
        return true;
    }

    auto http_server::handle_get_block_tx(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        if(!params.isArray() || params.size() < 2 || !params[0].isString()
           || !params[1].isString()) {
            m_log->warn("Invalid parameters to "
                        "getTransacionByBlock{Hash/Number}AndIndex");
            return false;
        }

        auto shadow_params = Json::Value(Json::arrayValue);
        shadow_params.append(params[0]);

        return fetch_block(
            shadow_params,
            callback,
            [this, callback, params](interface::exec_return_type res,
                                     const cbdc::buffer& runner_params) {
                auto& updates = std::get<return_type>(res);
                auto it = updates.find(runner_params);
                auto ret = Json::Value();
                if(it == updates.end() || it->second.size() == 0) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::not_found;
                    ret["error"]["message"] = "Data was not found";
                    callback(ret);
                    return;
                }

                auto maybe_pretend_block
                    = cbdc::from_buffer<evm_pretend_block>(it->second);
                if(!maybe_pretend_block) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::internal_error;
                    ret["error"]["message"] = "Internal error";
                    callback(ret);
                    return;
                }

                auto maybe_idx256 = uint256be_from_hex(params[1].asString());
                if(!maybe_idx256) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"]
                        = error_code::invalid_transaction_index;
                    ret["error"]["message"]
                        = "Transaction index was invalid - expect hex format";
                    callback(ret);
                    return;
                }
                auto idx = to_uint64(maybe_idx256.value());
                auto blk = maybe_pretend_block.value();
                if(blk.m_transactions.size() < idx) {
                    ret["error"] = Json::Value();
                    ret["error"]["code"] = error_code::not_found;
                    ret["error"]["message"] = "Data was not found";
                    callback(ret);
                    return;
                }

                auto json_tx
                    = tx_to_json(blk.m_transactions[idx].m_tx, m_secp);
                auto tn256 = evmc::uint256be(blk.m_ticket_number);
                json_tx["blockHash"] = "0x" + to_hex(tn256);
                json_tx["blockNumber"] = to_hex_trimmed(tn256);
                json_tx["transactionIndex"] = "0x0";
                ret["result"] = json_tx;
                callback(ret);
            });
    }

    auto http_server::handle_accounts(
        const Json::Value& /*params*/,
        const server_type::result_callback_type& callback) -> bool {
        auto ret = Json::Value();
        ret["result"] = Json::Value(Json::arrayValue);
        callback(ret);
        return true;
    }

    auto http_server::handle_estimate_gas(
        const Json::Value& /*params*/,
        const server_type::result_callback_type& callback) -> bool {
        auto ret = Json::Value();

        // TODO: actually estimate gas
        ret["result"] = "0xffffffffff";
        callback(ret);
        return true;
    }

    auto http_server::handle_client_version(
        const Json::Value& /*params*/,
        const server_type::result_callback_type& callback) -> bool {
        auto ret = Json::Value();
        ret["result"] = "opencbdc/v0.0";
        callback(ret);
        return true;
    }

    auto http_server::handle_gas_price(
        const Json::Value& /*params*/,
        const server_type::result_callback_type& callback) -> bool {
        auto ret = Json::Value();
        ret["result"] = "0x0";
        callback(ret);
        return true;
    }

    auto
    http_server::handle_call(Json::Value params,
                             const server_type::result_callback_type& callback)
        -> bool {
        if(!params.isArray() || params.empty() || !params[0].isObject()) {
            m_log->warn("Parameter to call is invalid");
            return false;
        }

        auto maybe_tx = dryrun_tx_from_json(params[0]);
        if(!maybe_tx) {
            m_log->warn("Parameter is not a valid transaction");
            return false;
        }

        auto& tx = maybe_tx.value();
        auto runner_params = make_buffer(*tx);

        return exec_tx(callback,
                       runner::evm_runner_function::dryrun_transaction,
                       runner_params,
                       true,
                       [callback, tx](interface::exec_return_type res) {
                           auto ret = Json::Value();
                           auto txid = cbdc::make_buffer(tx_id(tx->m_tx));

                           auto& updates = std::get<return_type>(res);
                           auto it = updates.find(txid);
                           if(it == updates.end()) {
                               ret["error"] = Json::Value();
                               ret["error"]["code"] = error_code::not_found;
                               ret["error"]["message"] = "Data was not found";
                               callback(ret);
                               return;
                           }

                           auto maybe_receipt
                               = cbdc::from_buffer<evm_tx_receipt>(it->second);
                           if(!maybe_receipt) {
                               ret["error"] = Json::Value();
                               ret["error"]["code"]
                                   = error_code::internal_error;
                               ret["error"]["message"] = "Internal error";
                               callback(ret);
                               return;
                           }

                           auto buf = cbdc::buffer();
                           buf.extend(maybe_receipt->m_output_data.size());
                           std::memcpy(buf.data(),
                                       maybe_receipt->m_output_data.data(),
                                       maybe_receipt->m_output_data.size());
                           ret["result"] = "0x" + buf.to_hex();
                           callback(ret);
                       });
    }

    auto http_server::handle_send_transaction(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        if(!params.isArray() || params.empty() || !params[0].isObject()) {
            m_log->warn("Invalid parameters to sendTransaction");
            return false;
        }

        auto maybe_tx = tx_from_json(params[0]);
        if(!maybe_tx) {
            m_log->warn("Parameter is not a valid transaction");
            return false;
        }

        auto& tx = maybe_tx.value();
        auto runner_params = make_buffer(*tx);
        return exec_tx(callback,
                       runner::evm_runner_function::execute_transaction,
                       runner_params,
                       false,
                       [callback, tx](const interface::exec_return_type&) {
                           auto txid = cbdc::make_buffer(tx_id(*tx));
                           auto ret = Json::Value();
                           ret["result"] = "0x" + txid.to_hex();
                           callback(ret);
                       });
    }

    auto http_server::exec_tx(
        const server_type::result_callback_type& callback,
        runner::evm_runner_function f_type,
        cbdc::buffer& runner_params,
        bool is_readonly_run,
        std::function<void(interface::exec_return_type)> res_cb) -> bool {
        auto function = cbdc::buffer();
        function.append(&f_type, sizeof(f_type));
        auto cb = [res_cb, callback](interface::exec_return_type res) {
            if(!std::holds_alternative<return_type>(res)) {
                auto ec = std::get<interface::error_code>(res);
                auto ret = Json::Value();
                ret["error"] = Json::Value();
                ret["error"]["code"]
                    = error_code::execution_error - static_cast<int>(ec);
                ret["error"]["message"] = "Execution error";
                callback(ret);
                return;
            }

            res_cb(res);
        };

        auto id = m_next_id++;
        auto a = [&]() {
            auto agent = std::make_shared<impl>(
                m_log,
                m_cfg,
                &runner::factory<runner::evm_runner>::create,
                m_broker,
                function,
                runner_params,
                [this, id, res_cb](interface::exec_return_type res) {
                    auto success = std::holds_alternative<return_type>(res);
                    if(!success) {
                        auto ec = std::get<interface::error_code>(res);
                        if(ec == interface::error_code::retry) {
                            m_retry_queue.push(id);
                            return;
                        }
                    }
                    res_cb(res);
                    m_cleanup_queue.push(id);
                },
                runner::evm_runner::initial_lock_type,
                is_readonly_run,
                m_secp,
                m_threads);
            {
                std::unique_lock l(m_agents_mut);
                m_agents.emplace(id, agent);
            }
            return agent;
        }();
        return a->exec();
    }
}
