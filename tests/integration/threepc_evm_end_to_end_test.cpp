// Copyright (c) 2023 MIT Digital Currency Initiative,
//
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "3pc/agent/impl.hpp"
#include "3pc/agent/runners/evm/address.hpp"
#include "3pc/agent/runners/evm/http_server.hpp"
#include "3pc/agent/runners/evm/math.hpp"
#include "3pc/agent/runners/evm/rlp.hpp"
#include "3pc/agent/runners/evm/signature.hpp"
#include "3pc/agent/runners/evm/util.hpp"
#include "3pc/broker/impl.hpp"
#include "3pc/directory/impl.hpp"
#include "3pc/runtime_locking_shard/impl.hpp"
#include "3pc/ticket_machine/impl.hpp"
#include "gtest_evm_jsonrpc_client.hpp"
#include "sample_erc20_contract.hpp"

#include <gtest/gtest.h>
#include <secp256k1.h>

static void gtest_erc20_output_hex_to_ascii(const std::string& hex,
                                            std::string& out_asciistr) {
    out_asciistr.clear();
    const auto len = hex.length();
    ASSERT_TRUE(len % 2 == 0);
    unsigned idx = 2 + 128;
    for(; idx < len; idx += 2) {
        std::string byte = hex.substr(idx, 2);
        const int chri = std::stoi(byte, nullptr, 16);
        if(chri != 0) {
            out_asciistr.push_back((char)chri);
        }
    }
}

static std::string gtest_descr() {
    // e.g.:  "GTEST: threepc_evm_end_to_end_test.native_transfer"
    return std::string("GTEST: ")
         + ::testing::UnitTest::GetInstance()
               ->current_test_info()
               ->test_suite_name()
         + "."
         + ::testing::UnitTest::GetInstance()->current_test_info()->name();
}

class threepc_evm_end_to_end_test : public ::testing::Test {
  protected:
    void SetUp() override {
        m_log->debug("threepc_evm_end_to_end_test::Setup()");
        const auto rpc_server_endpoint
            = cbdc::network::endpoint_t{"127.0.0.1", 7007};

        init_jsonrpc_server_and_client(rpc_server_endpoint);

        init_accounts();
    }

    void init_accounts();

    void init_jsonrpc_server_and_client(
        const cbdc::network::endpoint_t& rpc_server_endpoint);

    void test_erc20_name(const evmc::address& contract_address,
                         const evmc::address& from_address,
                         const cbdc::privkey_t& from_privkey,
                         const std::string& expected_name);

    void test_erc20_symbol(const evmc::address& contract_address,
                           const evmc::address& from_address,
                           const cbdc::privkey_t& from_privkey,
                           const std::string& expected_symbol);

    void test_erc20_deploy_contract(const evmc::address& from_addr,
                                    const cbdc::privkey_t& from_privkey,
                                    evmc::address& out_contract_address);

    void test_erc20_send_tokens(const evmc::address& contract_addr,
                                const evmc::address& from_addr,
                                const cbdc::privkey_t& from_privkey,
                                const evmc::address& to_addr,
                                const evmc::uint256be& erc20_value);

    void test_erc20_get_balance(const evmc::address& contract_address,
                                const evmc::address& acct_address,
                                const cbdc::privkey_t& acct_privkey,
                                const evmc::uint256be& expected_balance);

    void test_erc20_decimals(const evmc::address& contract_address,
                             const evmc::address& from_address,
                             const cbdc::privkey_t& from_privkey,
                             const evmc::uint256be& expected_decimals);

    void test_erc20_total_supply(const evmc::address& contract_address,
                                 const evmc::address& from_address,
                                 const cbdc::privkey_t& from_privkey,
                                 const evmc::uint256be& expected_total_supply);

    // Used by to execute transaction where where one single result str output
    // data is expected in the tx receipt.
    void
    test_erc20_tx_get_raw_output_data_(const evmc::address& contract_address,
                                       const evmc::address& from_address,
                                       const cbdc::privkey_t& from_privkey,
                                       const cbdc::buffer& input_data,
                                       std::string& out_raw_output_data);

    std::shared_ptr<cbdc::logging::log> m_log{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::info)};
    cbdc::threepc::config m_cfg{};
    std::shared_ptr<cbdc::threepc::broker::interface> m_broker;

    std::unique_ptr<cbdc::threepc::agent::rpc::http_server> m_rpc_server;
    std::shared_ptr<cbdc::test::gtest_evm_jsonrpc_client> m_rpc_client;

    std::shared_ptr<secp256k1_context> m_secp_context{
        secp256k1_context_create(SECP256K1_CONTEXT_SIGN
                                 | SECP256K1_CONTEXT_VERIFY),
        &secp256k1_context_destroy};

    cbdc::privkey_t m_acct0_privkey;
    cbdc::privkey_t m_acct1_privkey;
    evmc::address m_acct0_ethaddr;
    evmc::address m_acct1_ethaddr;

    evmc::uint256be m_init_acct_balance;
};

void threepc_evm_end_to_end_test::init_accounts() {
    // 1x10^24: i.e. 1mm with 18 decimals:
    m_init_acct_balance = cbdc::threepc::agent::runner::uint256be_from_hex(
                              "0xd3c21bcecceda1000000")
                              .value();

    m_acct0_privkey = cbdc::hash_from_hex(
        "96c92064b84b7a4e8f32f66014b1ba431c8fdf4382749328310cc9ec765bb76a");
    m_acct1_privkey = cbdc::hash_from_hex(
        "4bfb9012977703f9b30e8a8e98ce77f2c01e93b8dc6f46159162d5c6560e4e89");

    m_acct0_ethaddr = cbdc::threepc::agent::runner::eth_addr(m_acct0_privkey,
                                                             m_secp_context);
    m_acct1_ethaddr = cbdc::threepc::agent::runner::eth_addr(m_acct1_privkey,
                                                             m_secp_context);

    const auto check_ok_cb = [](bool ok) {
        ASSERT_TRUE(ok);
    };

    // Initialize native balances:
    auto acc1 = cbdc::threepc::agent::runner::evm_account();
    acc1.m_balance = evmc::uint256be(m_init_acct_balance);
    cbdc::threepc::put_row(m_broker,
                           cbdc::make_buffer(m_acct0_ethaddr),
                           cbdc::make_buffer(acc1),
                           check_ok_cb);

    auto acc2 = cbdc::threepc::agent::runner::evm_account();
    acc2.m_balance = evmc::uint256be(m_init_acct_balance);
    cbdc::threepc::put_row(m_broker,
                           cbdc::make_buffer(m_acct1_ethaddr),
                           cbdc::make_buffer(acc2),
                           check_ok_cb);
}

void threepc_evm_end_to_end_test::init_jsonrpc_server_and_client(
    const cbdc::network::endpoint_t& rpc_server_endpoint) {
    auto shards = std::vector<
        std::shared_ptr<cbdc::threepc::runtime_locking_shard::interface>>(
        {std::make_shared<cbdc::threepc::runtime_locking_shard::impl>(m_log)});

    m_broker = std::make_shared<cbdc::threepc::broker::impl>(
        0,
        shards,
        std::make_shared<cbdc::threepc::ticket_machine::impl>(m_log, 1),
        std::make_shared<cbdc::threepc::directory::impl>(1),
        m_log);

    m_rpc_server = std::make_unique<cbdc::threepc::agent::rpc::http_server>(
        std::make_unique<cbdc::rpc::json_rpc_http_server>(rpc_server_endpoint,
                                                          true),
        m_broker,
        m_log,
        m_cfg);

    ASSERT_TRUE(m_rpc_server->init());

    m_rpc_client = std::make_shared<cbdc::test::gtest_evm_jsonrpc_client>(
        std::vector<std::string>{"http://" + rpc_server_endpoint.first + ":"
                                 + std::to_string(rpc_server_endpoint.second)},
        0,
        m_log);
}

void threepc_evm_end_to_end_test::test_erc20_tx_get_raw_output_data_(
    const evmc::address& contract_address,
    const evmc::address& from_address,
    const cbdc::privkey_t& from_privkey,
    const cbdc::buffer& input_data,
    std::string& out_raw_output_data) {
    const auto make_evmtx_ = [&]() -> cbdc::threepc::agent::runner::evm_tx {
        auto etx = cbdc::threepc::agent::runner::evm_tx();
        etx.m_to = contract_address;
        etx.m_nonce = m_rpc_client->get_transaction_count(from_address);
        // NOTE etx.m_value is unset
        etx.m_gas_price = evmc::uint256be(0);
        etx.m_gas_limit = evmc::uint256be(0xffffffff);

        etx.m_input.resize(input_data.size());
        std::memcpy(etx.m_input.data(), input_data.data(), input_data.size());

        auto sighash = cbdc::threepc::agent::runner::sig_hash(etx);
        etx.m_sig = cbdc::threepc::agent::runner::eth_sign(from_privkey,
                                                           sighash,
                                                           etx.m_type,
                                                           m_secp_context);
        return etx;
    };

    m_log->info(gtest_descr(),
                std::string(__FUNCTION__) + "()",
                "From:",
                cbdc::threepc::agent::runner::to_hex(from_address),
                "Contract:",
                cbdc::threepc::agent::runner::to_hex(contract_address));

    const auto etx = make_evmtx_();

    // Send the transaction:
    std::string txid{};
    m_rpc_client->send_transaction(etx, txid);

    // Retrieve the receipt and check it:
    Json::Value txreceipt;
    m_rpc_client->get_transaction_receipt(txid, txreceipt);

    ASSERT_TRUE(txreceipt.isMember("from"));
    ASSERT_EQ(txreceipt["from"],
              "0x" + cbdc::threepc::agent::runner::to_hex(from_address));

    ASSERT_TRUE(txreceipt.isMember("to"));
    ASSERT_EQ(txreceipt["to"],
              "0x" + cbdc::threepc::agent::runner::to_hex(contract_address));

    ASSERT_TRUE(txreceipt.isMember("transactionHash"));
    ASSERT_EQ(txreceipt["transactionHash"], txid);

    ASSERT_TRUE(txreceipt.isMember("status"));
    ASSERT_EQ(txreceipt["status"], "0x1");

    ASSERT_TRUE(txreceipt.isMember("success"));
    ASSERT_EQ(txreceipt["success"], "0x1");

    ASSERT_TRUE(txreceipt.isMember("transaction"));
    ASSERT_TRUE(txreceipt["transaction"].isMember("value"));
    ASSERT_EQ(cbdc::threepc::agent::runner::uint256be_from_hex(
                  txreceipt["transaction"]["value"].asString()),
              etx.m_value);

    ASSERT_TRUE(txreceipt.isMember("output_data"));
    out_raw_output_data = txreceipt["output_data"].asString();
}

void threepc_evm_end_to_end_test::test_erc20_name(
    const evmc::address& contract_address,
    const evmc::address& from_address,
    const cbdc::privkey_t& from_privkey,
    const std::string& expected_name) {
    m_log->info(gtest_descr(),
                std::string(__FUNCTION__) + "()",
                "Confirming that contract name is:",
                expected_name);
    std::string raw_output_data;
    test_erc20_tx_get_raw_output_data_(
        contract_address,
        from_address,
        from_privkey,
        cbdc::test::evm_contracts::data_erc20_name(),
        raw_output_data);

    std::string formatted_resultstr;
    gtest_erc20_output_hex_to_ascii(raw_output_data, formatted_resultstr);
    ASSERT_EQ(formatted_resultstr, expected_name);
}

void threepc_evm_end_to_end_test::test_erc20_symbol(
    const evmc::address& contract_address,
    const evmc::address& from_address,
    const cbdc::privkey_t& from_privkey,
    const std::string& expected_symbol) {
    m_log->info(gtest_descr(),
                std::string(__FUNCTION__) + "()",
                "Confirming that contract symbol is:",
                expected_symbol);
    std::string raw_output_data;
    test_erc20_tx_get_raw_output_data_(
        contract_address,
        from_address,
        from_privkey,
        cbdc::test::evm_contracts::data_erc20_symbol(),
        raw_output_data);

    std::string formatted_resultstr;
    gtest_erc20_output_hex_to_ascii(raw_output_data, formatted_resultstr);
    ASSERT_EQ(formatted_resultstr, expected_symbol);
}

void threepc_evm_end_to_end_test::test_erc20_decimals(
    const evmc::address& contract_address,
    const evmc::address& from_address,
    const cbdc::privkey_t& from_privkey,
    const evmc::uint256be& expected_decimals) {
    m_log->info(
        gtest_descr(),
        std::string(__FUNCTION__) + "()",
        "Confirming that number of decimals is:",
        cbdc::threepc::agent::runner::to_hex_trimmed(expected_decimals));
    std::string raw_output_data;
    test_erc20_tx_get_raw_output_data_(
        contract_address,
        from_address,
        from_privkey,
        cbdc::test::evm_contracts::data_erc20_decimals(),
        raw_output_data);

    const auto decimals
        = cbdc::threepc::agent::runner::uint256be_from_hex(raw_output_data);
    ASSERT_TRUE(decimals.has_value());
    ASSERT_EQ(decimals.value(), expected_decimals);
}

void threepc_evm_end_to_end_test::test_erc20_total_supply(
    const evmc::address& contract_address,
    const evmc::address& from_address,
    const cbdc::privkey_t& from_privkey,
    const evmc::uint256be& expected_total_supply) {
    m_log->info(
        gtest_descr(),
        std::string(__FUNCTION__) + "()",
        "Confirming that total supply is:",
        cbdc::threepc::agent::runner::to_hex_trimmed(expected_total_supply));
    std::string raw_output_data;
    test_erc20_tx_get_raw_output_data_(
        contract_address,
        from_address,
        from_privkey,
        cbdc::test::evm_contracts::data_erc20_total_supply(),
        raw_output_data);

    const auto decimals
        = cbdc::threepc::agent::runner::uint256be_from_hex(raw_output_data);
    ASSERT_TRUE(decimals.has_value());
    ASSERT_EQ(decimals.value(), expected_total_supply);
}

void threepc_evm_end_to_end_test::test_erc20_deploy_contract(
    const evmc::address& from_addr,
    const cbdc::privkey_t& from_privkey,
    evmc::address& out_contract_address) {
    const auto make_evmtx_ = [&]() -> cbdc::threepc::agent::runner::evm_tx {
        auto etx = cbdc::threepc::agent::runner::evm_tx();
        // NOTE etx.m_to is empty for contract deployment
        etx.m_nonce = m_rpc_client->get_transaction_count(from_addr);
        // NOTE etx.m_value is unset
        etx.m_gas_price = evmc::uint256be(0);
        etx.m_gas_limit = evmc::uint256be(0xffffffff);

        const auto& contract_bytecode
            = cbdc::test::evm_contracts::data_erc20_contract_bytecode();
        etx.m_input.resize(contract_bytecode.size());
        std::memcpy(etx.m_input.data(),
                    contract_bytecode.data(),
                    contract_bytecode.size());

        auto sighash = cbdc::threepc::agent::runner::sig_hash(etx);
        etx.m_sig = cbdc::threepc::agent::runner::eth_sign(from_privkey,
                                                           sighash,
                                                           etx.m_type,
                                                           m_secp_context);
        return etx;
    };

    const auto etx = make_evmtx_();

    const auto maybe_from
        = cbdc::threepc::agent::runner::check_signature(etx, m_secp_context);
    ASSERT_TRUE(maybe_from.has_value());
    ASSERT_EQ(maybe_from.value(), from_addr);

    // Send the transaction:
    std::string txid{};
    m_rpc_client->send_transaction(etx, txid);

    const auto expected_contract_address
        = cbdc::threepc::agent::runner::contract_address(from_addr,
                                                         etx.m_nonce);
    m_log->info(
        gtest_descr(),
        std::string(__FUNCTION__) + "()",
        "Owner:",
        cbdc::threepc::agent::runner::to_hex(from_addr),
        "Contract Addr:",
        cbdc::threepc::agent::runner::to_hex(expected_contract_address));

    // Retrieve the receipt and check it:
    Json::Value txreceipt;
    m_rpc_client->get_transaction_receipt(txid, txreceipt);

    ASSERT_TRUE(txreceipt.isMember("from"));
    ASSERT_EQ(txreceipt["from"],
              "0x" + cbdc::threepc::agent::runner::to_hex(from_addr));

    ASSERT_TRUE(txreceipt.isMember("contractAddress"));
    ASSERT_EQ(
        txreceipt["contractAddress"],
        "0x"
            + cbdc::threepc::agent::runner::to_hex(expected_contract_address));

    ASSERT_TRUE(txreceipt.isMember("to"));
    ASSERT_TRUE(txreceipt["to"].isNull());

    ASSERT_TRUE(txreceipt.isMember("transactionHash"));
    ASSERT_EQ(txreceipt["transactionHash"], txid);

    ASSERT_TRUE(txreceipt.isMember("status"));
    ASSERT_EQ(txreceipt["status"], "0x1");

    ASSERT_TRUE(txreceipt.isMember("success"));
    ASSERT_EQ(txreceipt["success"], "0x1");

    ASSERT_TRUE(txreceipt.isMember("transaction"));
    ASSERT_TRUE(txreceipt["transaction"].isMember("value"));
    ASSERT_EQ(cbdc::threepc::agent::runner::uint256be_from_hex(
                  txreceipt["transaction"]["value"].asString()),
              etx.m_value);

    out_contract_address = expected_contract_address;
}

void threepc_evm_end_to_end_test::test_erc20_get_balance(
    const evmc::address& contract_address,
    const evmc::address& acct_address,
    const cbdc::privkey_t& acct_privkey,
    const evmc::uint256be& expected_balance) {
    m_log->info(
        gtest_descr(),
        std::string(__FUNCTION__) + "()",
        "Confirming that balance of address",
        cbdc::threepc::agent::runner::to_hex(acct_address),
        "is",
        cbdc::threepc::agent::runner::to_hex_trimmed(expected_balance));

    std::string raw_output_data;
    test_erc20_tx_get_raw_output_data_(
        contract_address,
        acct_address,
        acct_privkey,
        cbdc::test::evm_contracts::data_erc20_balance_of(acct_address),
        raw_output_data);

    const auto decimals
        = cbdc::threepc::agent::runner::uint256be_from_hex(raw_output_data);
    ASSERT_TRUE(decimals.has_value());
    ASSERT_EQ(decimals.value(), expected_balance);
}

void threepc_evm_end_to_end_test::test_erc20_send_tokens(
    const evmc::address& contract_address,
    const evmc::address& from_address,
    const cbdc::privkey_t& from_privkey,
    const evmc::address& to_address,
    const evmc::uint256be& erc20_value) {
    const auto make_evmtx_ = [&]() -> cbdc::threepc::agent::runner::evm_tx {
        auto etx = cbdc::threepc::agent::runner::evm_tx();
        etx.m_to = contract_address;
        etx.m_nonce = m_rpc_client->get_transaction_count(from_address);
        // NOTE etx.m_value is unset
        etx.m_gas_price = evmc::uint256be(0);
        etx.m_gas_limit = evmc::uint256be(0xffffffff);

        const cbdc::buffer& input_data
            = cbdc::test::evm_contracts::data_erc20_transfer(to_address,
                                                             erc20_value);
        etx.m_input.resize(input_data.size());
        std::memcpy(etx.m_input.data(), input_data.data(), input_data.size());

        auto sighash = cbdc::threepc::agent::runner::sig_hash(etx);
        etx.m_sig = cbdc::threepc::agent::runner::eth_sign(from_privkey,
                                                           sighash,
                                                           etx.m_type,
                                                           m_secp_context);
        return etx;
    };

    m_log->info(gtest_descr(),
                std::string(__FUNCTION__) + "()",
                "From:",
                cbdc::threepc::agent::runner::to_hex(from_address),
                "To:",
                cbdc::threepc::agent::runner::to_hex(to_address),
                "Contract:",
                cbdc::threepc::agent::runner::to_hex(contract_address));

    const auto etx = make_evmtx_();

    // Send the transaction:
    std::string txid{};
    m_rpc_client->send_transaction(etx, txid);

    // Retrieve the receipt and check it:
    Json::Value txreceipt;
    m_rpc_client->get_transaction_receipt(txid, txreceipt);

    ASSERT_TRUE(txreceipt.isMember("from"));
    ASSERT_EQ(txreceipt["from"],
              "0x" + cbdc::threepc::agent::runner::to_hex(from_address));

    ASSERT_TRUE(txreceipt.isMember("to"));
    ASSERT_EQ(txreceipt["to"],
              "0x" + cbdc::threepc::agent::runner::to_hex(contract_address));

    ASSERT_TRUE(txreceipt.isMember("transactionHash"));
    ASSERT_EQ(txreceipt["transactionHash"], txid);

    ASSERT_TRUE(txreceipt.isMember("status"));
    ASSERT_EQ(txreceipt["status"], "0x1");

    ASSERT_TRUE(txreceipt.isMember("success"));
    ASSERT_EQ(txreceipt["success"], "0x1");

    ASSERT_TRUE(txreceipt.isMember("transaction"));
    ASSERT_TRUE(txreceipt["transaction"].isMember("value"));
    ASSERT_EQ(cbdc::threepc::agent::runner::uint256be_from_hex(
                  txreceipt["transaction"]["value"].asString()),
              etx.m_value);

    ASSERT_TRUE(txreceipt.isMember("logs"));
    ASSERT_TRUE(txreceipt["logs"].isArray());
    ASSERT_TRUE(txreceipt["logs"].size() == 1);
    ASSERT_TRUE(txreceipt["logs"][0].isMember("data"));
    ASSERT_TRUE(txreceipt["logs"][0]["data"].isString());
    ASSERT_EQ(txreceipt["logs"][0]["data"].asString(),
              "0x" + cbdc::threepc::agent::runner::to_hex(erc20_value));

    ASSERT_TRUE(txreceipt["logs"][0].isMember("address"));
    ASSERT_TRUE(txreceipt["logs"][0]["address"].isString());
    ASSERT_EQ(txreceipt["logs"][0]["address"].asString(),
              "0x" + cbdc::threepc::agent::runner::to_hex(contract_address));

    ASSERT_TRUE(txreceipt["logs"][0].isMember("transactionHash"));
    ASSERT_TRUE(txreceipt["logs"][0]["transactionHash"].isString());
    ASSERT_EQ(txreceipt["logs"][0]["transactionHash"].asString(), txid);
}

TEST_F(threepc_evm_end_to_end_test, native_transfer) {
    const auto send_value{evmc::uint256be(1000)};
    const auto make_evmtx_ = [&]() -> cbdc::threepc::agent::runner::evm_tx {
        auto etx = cbdc::threepc::agent::runner::evm_tx();
        etx.m_to = m_acct1_ethaddr;
        etx.m_nonce = m_rpc_client->get_transaction_count(m_acct0_ethaddr);
        etx.m_value = send_value;
        etx.m_gas_price = evmc::uint256be(0);
        etx.m_gas_limit = evmc::uint256be(0xffffffff);
        auto sighash = cbdc::threepc::agent::runner::sig_hash(etx);
        etx.m_sig = cbdc::threepc::agent::runner::eth_sign(m_acct0_privkey,
                                                           sighash,
                                                           etx.m_type,
                                                           m_secp_context);
        return etx;
    };

    const auto etx = make_evmtx_();

    const auto maybe_from
        = cbdc::threepc::agent::runner::check_signature(etx, m_secp_context);
    ASSERT_TRUE(maybe_from.has_value());
    ASSERT_EQ(maybe_from.value(), m_acct0_ethaddr);

    // Send the transaction:
    std::string txid{};
    m_rpc_client->send_transaction(etx, txid);

    // Retrieve the receipt and check it:
    Json::Value txreceipt;
    m_rpc_client->get_transaction_receipt(txid, txreceipt);

    ASSERT_TRUE(txreceipt.isMember("from"));
    ASSERT_EQ(txreceipt["from"],
              "0x" + cbdc::threepc::agent::runner::to_hex(m_acct0_ethaddr));

    ASSERT_TRUE(txreceipt.isMember("to"));
    ASSERT_EQ(txreceipt["to"],
              "0x" + cbdc::threepc::agent::runner::to_hex(etx.m_to.value()));

    ASSERT_TRUE(txreceipt.isMember("transactionHash"));
    ASSERT_EQ(txreceipt["transactionHash"], txid);

    ASSERT_TRUE(txreceipt.isMember("status"));
    ASSERT_EQ(txreceipt["status"], "0x0");

    ASSERT_TRUE(txreceipt.isMember("success"));
    ASSERT_EQ(txreceipt["success"], "0x1");

    ASSERT_TRUE(txreceipt.isMember("transaction"));
    ASSERT_TRUE(txreceipt["transaction"].isMember("value"));
    ASSERT_EQ(cbdc::threepc::agent::runner::uint256be_from_hex(
                  txreceipt["transaction"]["value"].asString()),
              etx.m_value);

    // Check resulting balance:
    std::optional<evmc::uint256be> sender_balance;
    m_rpc_client->get_balance(m_acct0_ethaddr, sender_balance);
    ASSERT_TRUE(sender_balance.has_value());
    ASSERT_EQ(sender_balance.value(),
              cbdc::threepc::agent::runner::operator-(m_init_acct_balance,
                                                      send_value));

    std::optional<evmc::uint256be> receiver_balance;
    m_rpc_client->get_balance(etx.m_to.value(), receiver_balance);
    ASSERT_TRUE(receiver_balance.has_value());
    ASSERT_EQ(receiver_balance.value(),
              cbdc::threepc::agent::runner::operator+(m_init_acct_balance,
                                                      send_value));
}

TEST_F(threepc_evm_end_to_end_test, erc20_all) {
    evmc::address contract_address;
    test_erc20_deploy_contract(m_acct0_ethaddr,
                               m_acct0_privkey,
                               contract_address);

    test_erc20_name(contract_address,
                    m_acct0_ethaddr,
                    m_acct0_privkey,
                    "Tokens");

    test_erc20_symbol(contract_address,
                      m_acct0_ethaddr,
                      m_acct0_privkey,
                      "TOK");

    test_erc20_decimals(
        contract_address,
        m_acct0_ethaddr,
        m_acct0_privkey,
        cbdc::threepc::agent::runner::uint256be_from_hex("0x12").value());

    test_erc20_total_supply(contract_address,
                            m_acct0_ethaddr,
                            m_acct0_privkey,
                            m_init_acct_balance);

    test_erc20_get_balance(contract_address,
                           m_acct0_ethaddr,
                           m_acct0_privkey,
                           m_init_acct_balance);

    test_erc20_get_balance(contract_address,
                           m_acct1_ethaddr,
                           m_acct1_privkey,
                           evmc::uint256be{});

    const auto txfer_amount
        = cbdc::threepc::agent::runner::uint256be_from_hex("0xF423F").value();

    // Send ERC20 tokens from acct0 --> acct1 & confirm
    test_erc20_send_tokens(contract_address,
                           m_acct0_ethaddr,
                           m_acct0_privkey,
                           m_acct1_ethaddr,
                           txfer_amount);

    test_erc20_get_balance(
        contract_address,
        m_acct0_ethaddr,
        m_acct0_privkey,
        cbdc::threepc::agent::runner::operator-(m_init_acct_balance,
                                                txfer_amount));

    test_erc20_get_balance(contract_address,
                           m_acct1_ethaddr,
                           m_acct1_privkey,
                           txfer_amount);

    // Send ERC20 tokens back from acct1 --> acct0 & confirm
    test_erc20_send_tokens(contract_address,
                           m_acct1_ethaddr,
                           m_acct1_privkey,
                           m_acct0_ethaddr,
                           txfer_amount);

    test_erc20_get_balance(contract_address,
                           m_acct0_ethaddr,
                           m_acct0_privkey,
                           m_init_acct_balance);

    test_erc20_get_balance(contract_address,
                           m_acct1_ethaddr,
                           m_acct1_privkey,
                           evmc::uint256be{});
}
