// Copyright (c) 2023 MIT Digital Currency Initiative,
//
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest_evm_jsonrpc_client.hpp"

#include "3pc/agent/runners/evm/address.hpp"
#include "3pc/agent/runners/evm/rlp.hpp"
#include "3pc/agent/runners/evm/serialization.hpp"
#include "3pc/agent/runners/evm/util.hpp"

#include <gtest/gtest.h>
#include <thread>

namespace cbdc::test {
    static std::string gtest_descr() {
        // e.g.:  "GTEST: threepc_evm_end_to_end_test.native_transfer"
        return std::string("GTEST: ")
             + ::testing::UnitTest::GetInstance()
                   ->current_test_info()
                   ->test_suite_name()
             + "."
             + ::testing::UnitTest::GetInstance()->current_test_info()->name();
    }

    gtest_evm_jsonrpc_client::gtest_evm_jsonrpc_client(
        std::vector<std::string> endpoints,
        long timeout,
        const std::shared_ptr<cbdc::logging::log>& log)
        : cbdc::rpc::json_rpc_http_client(std::move(endpoints), timeout, log) {
    }

    evmc::uint256be gtest_evm_jsonrpc_client::get_transaction_count(
        const evmc::address& addr) {
        std::string txcount_str;
        get_transaction_count_str_(addr, txcount_str);
        m_log->debug(gtest_descr(),
                     std::string(__FUNCTION__) + "()",
                     "0x" + cbdc::threepc::agent::runner::to_hex(addr),
                     txcount_str);
        return cbdc::threepc::agent::runner::uint256be_from_hex(txcount_str)
            .value();
    }

    void gtest_evm_jsonrpc_client::get_transaction_count_str_(
        const evmc::address& addr,
        std::string& out_txcount_str) {
        auto params = Json::Value();
        params.append("0x" + cbdc::threepc::agent::runner::to_hex(addr));
        params.append("latest");

        std::atomic<bool> tx_done{false};
        call("eth_getTransactionCount",
             std::move(params),
             [&tx_done, &out_txcount_str](std::optional<Json::Value> res) {
                 ASSERT_TRUE(res.has_value());
                 const auto& v = res.value();
                 ASSERT_FALSE(v.isMember(m_json_error_key));
                 ASSERT_TRUE(v.isMember(m_json_result_key));
                 out_txcount_str = v[m_json_result_key].asString();
                 ASSERT_TRUE(out_txcount_str.length() > 0);

                 tx_done = true;
             });

        for(int cnt = 0; cnt < 20 && !tx_done; ++cnt) {
            ASSERT_TRUE(pump());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        ASSERT_TRUE(tx_done);
    }

    void gtest_evm_jsonrpc_client::send_transaction(
        const cbdc::threepc::agent::runner::evm_tx& etx,
        std::string& out_txid) {
        const auto rlp_tx_buf = cbdc::threepc::agent::runner::tx_encode(etx);
        const auto rlp_tx_hex = "0x" + rlp_tx_buf.to_hex();

        auto params = Json::Value();
        params.append(rlp_tx_hex);

        std::atomic<bool> tx_done{false};
        std::string txid{};
        call("eth_sendRawTransaction",
             std::move(params),
             [&tx_done, &out_txid](std::optional<Json::Value> res) {
                 ASSERT_TRUE(res.has_value());
                 const auto& v = res.value();
                 ASSERT_FALSE(v.isMember(m_json_error_key));
                 ASSERT_TRUE(v.isMember(m_json_result_key));

                 ASSERT_TRUE(v.size() == 3);
                 ASSERT_TRUE(v.isMember("id"));
                 ASSERT_TRUE(v["id"].isInt());
                 ASSERT_TRUE(v.isMember("jsonrpc"));
                 ASSERT_TRUE(v["jsonrpc"].isString()); // e.g. "2.0"

                 ASSERT_TRUE(v[m_json_result_key].isString());
                 out_txid = v[m_json_result_key].asString();
                 ASSERT_TRUE(out_txid.length() > 0);

                 tx_done = true;
             });

        for(int cnt = 0; cnt < 20 && !tx_done; ++cnt) {
            ASSERT_TRUE(pump());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        ASSERT_TRUE(tx_done);
    }

    void gtest_evm_jsonrpc_client::get_transaction_receipt(
        const std::string& txid,
        Json::Value& out_tx_receipt) {
        auto params = Json::Value();
        params.append(txid);

        std::atomic<bool> tx_done{false};
        call(
            "eth_getTransactionReceipt",
            std::move(params),
            [&out_tx_receipt, &tx_done, this](std::optional<Json::Value> res) {
                ASSERT_TRUE(res.has_value());
                auto& v = res.value();
                ASSERT_FALSE(v.isMember(m_json_error_key));
                ASSERT_TRUE(v.isMember(m_json_result_key));

                out_tx_receipt = v[m_json_result_key];
                ASSERT_TRUE(out_tx_receipt.isObject());

                for(auto const& id : out_tx_receipt.getMemberNames()) {
                    m_log->debug(
                        "gtest_evm_jsonrpc_client::get_transaction_receipt() "
                        "json::value member:",
                        id,
                        out_tx_receipt[id].type(),
                        out_tx_receipt[id]);
                }

                tx_done = true;
            });

        for(int cnt = 0; cnt < 20 && !tx_done; ++cnt) {
            ASSERT_TRUE(pump());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        ASSERT_TRUE(tx_done);
    }

    void gtest_evm_jsonrpc_client::get_balance(
        const evmc::address& addr,
        std::optional<evmc::uint256be>& out_balance) {
        auto params = Json::Value();
        params.append("0x" + cbdc::threepc::agent::runner::to_hex(addr));

        std::atomic<bool> tx_done{false};
        call("eth_getBalance",
             std::move(params),
             [&tx_done, &out_balance](std::optional<Json::Value> res) {
                 ASSERT_TRUE(res.has_value());
                 const auto& v = res.value();
                 ASSERT_FALSE(v.isMember(m_json_error_key));
                 ASSERT_TRUE(v.isMember(m_json_result_key));

                 auto res_str = v[m_json_result_key].asString();
                 out_balance
                     = cbdc::threepc::agent::runner::uint256be_from_hex(
                         res_str);

                 tx_done = true;
             });

        for(int cnt = 0; cnt < 20 && !tx_done; ++cnt) {
            ASSERT_TRUE(pump());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        ASSERT_TRUE(tx_done);
        ASSERT_TRUE(out_balance.has_value());
    }
}
