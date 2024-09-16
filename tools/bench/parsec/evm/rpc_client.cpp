// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc_client.hpp"

geth_client::geth_client(std::vector<std::string> endpoints,
                         long timeout,
                         std::shared_ptr<cbdc::logging::log> log)
    : cbdc::rpc::json_rpc_http_client(std::move(endpoints),
                                      timeout,
                                      std::move(log)) {}

void geth_client::send_transaction(
    const std::string& tx,
    std::function<void(std::optional<std::string>)> cb) {
    auto params = Json::Value();
    params.append(tx);
    call("eth_sendRawTransaction",
         std::move(params),
         [this, c{std::move(cb)}](std::optional<Json::Value> res) {
             if(!res.has_value()) {
                 c(std::nullopt);
                 return;
             }

             auto& v = res.value();
             if(v.isMember(error_key)) {
                if (res->isObject()) {
                    m_log->trace("send_transaction Returns ", res->toStyledString());
                } else if (res->isString()) {
                    m_log->trace("send_transaction Returns: ", res->asString());
                } else {
                    m_log->trace("send_transaction Returns: Unexpected type");
                }

                 c(std::nullopt);
                 return;
             }

             if(v.isMember(result_key)) {
                 c(v[result_key].asString());
                 return;
             }

             c(std::nullopt);
         });
}


void geth_client::get_transaction_receipt(
    const std::string& tx,
    std::function<void(std::optional<std::string>)> cb) {
    auto params = Json::Value();
    //m_log->trace("Sending eth_getTransactionReceipt with param -> ", tx);
    params.append(tx);
    call("eth_getTransactionReceipt",
         std::move(params),
         [this, c{std::move(cb)}](std::optional<Json::Value> res) {
             if(!res.has_value()) {
                 c(std::nullopt);
                 return;
             }
            
             auto& v = res.value();
             if(v.isMember(error_key)) {
                 if(res->isObject()) {
                     m_log->trace("get_transaction_receipt Returns ",
                                  res->toStyledString());
                 } else if(res->isString()) {
                     m_log->trace("get_transaction_receipt Returns: ",
                                  res->asString());
                 } else {
                     m_log->trace(
                         "get_transaction_receipt Returns: Unexpected type");
                 }

                 c(std::nullopt);
                 return;
             }

             if(v.isMember(result_key)) {
                 c(v[result_key][output_data_key].asString());
                 return;
             }

             c(std::nullopt);
         });
}

void geth_client::get_transaction_count(
    const std::string& addr,
    std::function<void(std::optional<evmc::uint256be>)> cb) {
    auto params = Json::Value();
    params.append("0x" + addr);
    params.append("latest");
    call("eth_getTransactionCount",
         std::move(params),
         [c{std::move(cb)}](std::optional<Json::Value> res) {
             if(!res.has_value()) {
                 c(std::nullopt);
                 return;
             }

             auto& v = res.value();
             if(v.isMember(error_key)) {
                 c(std::nullopt);
                 return;
             }

             if(v.isMember(result_key)) {
                 auto res_str = v[result_key].asString();
                 c(cbdc::parsec::agent::runner::uint256be_from_hex(res_str));
                 return;
             }

             c(std::nullopt);
         });
}

// Calls eth_getBalance for the given address, with
/// the given callback function.
/// \param addr the address to find the count of transactions from
/// \param cb callback function passed to call()
void geth_client::get_balance(
    const std::string& addr,
    std::function<void(std::optional<evmc::uint256be>)> cb) {
    auto params = Json::Value();
    params.append("0x" + addr);
    params.append("latest");
    call("eth_getBalance",
         std::move(params),
         [c{std::move(cb)}](std::optional<Json::Value> res) {
             if(!res.has_value()) {
                 c(std::nullopt);
                 return;
             }

             auto& v = res.value();
             if(v.isMember(error_key)) {
                 c(std::nullopt);
                 return;
             }

             if(v.isMember(result_key)) {
                 auto res_str = v[result_key].asString();
                 c(cbdc::parsec::agent::runner::uint256be_from_hex(res_str));
                 return;
             }

             c(std::nullopt);
         });
}
