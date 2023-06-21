// Copyright (c) 2023 MIT Digital Currency Initiative,
//
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TEST_INTEGRATION_GTEST_EVM_JSONRPC_CLIENT_H_
#define OPENCBDC_TEST_INTEGRATION_GTEST_EVM_JSONRPC_CLIENT_H_

#include "parsec/agent/runners/evm/messages.hpp"
#include "util/rpc/http/json_rpc_http_client.hpp"

namespace cbdc::test {

    class gtest_evm_jsonrpc_client : public cbdc::rpc::json_rpc_http_client {
      public:
        gtest_evm_jsonrpc_client(
            std::vector<std::string> endpoints,
            long timeout,
            const std::shared_ptr<cbdc::logging::log>& log);

        void send_transaction(const cbdc::parsec::agent::runner::evm_tx& tx,
                              std::string& out_txid);

        void get_transaction_receipt(const std::string& txid,
                                     Json::Value& out_tx_receipt);

        void get_balance(const evmc::address& addr,
                         std::optional<evmc::uint256be>& out_balance);

        [[nodiscard]] evmc::uint256be
        get_transaction_count(const evmc::address& addr);

      private:
        static constexpr auto m_json_error_key = "error";
        static constexpr auto m_json_result_key = "result";

        void get_transaction_count_str_(const evmc::address& addr,
                                        std::string& out_txcount_str);
    };

}

#endif // OPENCBDC_TEST_INTEGRATION_GTEST_EVM_JSONRPC_CLIENT_H_
