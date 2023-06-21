// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_TOOLS_BENCH_PARSEC_EVM_RPC_CLIENT_H_
#define OPENCBDC_TX_TOOLS_BENCH_PARSEC_EVM_RPC_CLIENT_H_

#include "parsec/agent/runners/evm/util.hpp"
#include "util/rpc/http/json_rpc_http_client.hpp"

class geth_client : public cbdc::rpc::json_rpc_http_client {
  public:
    geth_client(std::vector<std::string> endpoints,
                long timeout,
                std::shared_ptr<cbdc::logging::log> log);

    static constexpr auto error_key = "error";
    static constexpr auto result_key = "result";

    /// Calls eth_sendRawTransaction with the given transaction and
    /// callback function.
    /// \param tx transaction bytecode
    /// \param cb callback function passed to call()
    void send_transaction(const std::string& tx,
                          std::function<void(std::optional<std::string>)> cb);

    /// Calls eth_getTransactionCount for the given address, with
    /// the given callback function.
    /// \param addr the address to find the count of transactions from
    /// \param cb callback function passed to call()
    void get_transaction_count(
        const std::string& addr,
        std::function<void(std::optional<evmc::uint256be>)> cb);
};

#endif
