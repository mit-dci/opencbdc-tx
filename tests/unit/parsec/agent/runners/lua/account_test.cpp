// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "../../../util.hpp"
#include "crypto/sha256.h"
#include "parsec/agent/impl.hpp"
#include "parsec/agent/runners/lua/impl.hpp"
#include "parsec/broker/impl.hpp"
#include "parsec/directory/impl.hpp"
#include "parsec/runtime_locking_shard/impl.hpp"
#include "parsec/ticket_machine/impl.hpp"
#include "util/common/keys.hpp"
#include "util/serialization/buffer_serializer.hpp"
#include "util/serialization/format.hpp"

#include <gtest/gtest.h>
#include <lua.hpp>
#include <secp256k1.h>
#include <secp256k1_schnorrsig.h>
#include <thread>

class account_test : public ::testing::Test {
  protected:
    void SetUp() override {
        m_pay_contract_key.append("pay", 3);

        lua_State* L = luaL_newstate();
        luaL_openlibs(L);
        luaL_dofile(
            L,
            "../tests/unit/parsec/agent/runners/lua/gen_pay_contract.lua");
        lua_getglobal(L, "gen_bytecode");
        ASSERT_EQ(lua_pcall(L, 0, 1, 0), 0);
        m_pay_contract = cbdc::buffer::from_hex(lua_tostring(L, -1)).value();

        cbdc::test::add_to_shard(m_broker, m_pay_contract_key, m_pay_contract);

        m_init_account_key.append("account_", 8);
        m_init_account_key.append(m_init_account_pkey.data(),
                                  m_init_account_pkey.size());
        auto init_account = cbdc::buffer();
        auto ser = cbdc::buffer_serializer(init_account);
        ser << m_init_balance << m_init_sequence;
        cbdc::test::add_to_shard(m_broker, m_init_account_key, init_account);
    }

    std::shared_ptr<cbdc::logging::log> m_log{
        std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::trace)};
    cbdc::parsec::config m_cfg{};
    std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface> m_shard0{
        std::make_shared<cbdc::parsec::runtime_locking_shard::impl>(m_log)};
    std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface> m_shard1{
        std::make_shared<cbdc::parsec::runtime_locking_shard::impl>(m_log)};
    std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface> m_shard2{
        std::make_shared<cbdc::parsec::runtime_locking_shard::impl>(m_log)};
    std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface> m_shard3{
        std::make_shared<cbdc::parsec::runtime_locking_shard::impl>(m_log)};
    std::shared_ptr<cbdc::parsec::ticket_machine::interface> m_ticketer{
        std::make_shared<cbdc::parsec::ticket_machine::impl>(m_log, 10)};
    std::shared_ptr<cbdc::parsec::directory::interface> m_directory{
        std::make_shared<cbdc::parsec::directory::impl>(4)};
    std::shared_ptr<cbdc::parsec::broker::interface> m_broker{
        std::make_shared<cbdc::parsec::broker::impl>(
            0,
            std::vector<std::shared_ptr<
                cbdc::parsec::runtime_locking_shard::interface>>(
                {m_shard0, m_shard1, m_shard2, m_shard3}),
            m_ticketer,
            m_directory,
            m_log)};

    cbdc::buffer m_pay_contract_key;
    cbdc::buffer m_pay_contract;
    cbdc::buffer m_init_account_key;
    static constexpr uint64_t m_init_balance{100};
    uint64_t m_init_sequence{0};
    cbdc::privkey_t m_init_account_skey{1};

    std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)>
        m_secp_context{secp256k1_context_create(SECP256K1_CONTEXT_SIGN),
                       &secp256k1_context_destroy};
    cbdc::pubkey_t m_init_account_pkey{
        cbdc::pubkey_from_privkey(m_init_account_skey, m_secp_context.get())};
};

TEST_F(account_test, pay_test) {
    auto params = cbdc::buffer();
    params.append(m_init_account_pkey.data(), m_init_account_pkey.size());

    auto account2_skey = cbdc::privkey_t{2};
    auto account2_pkey
        = cbdc::pubkey_from_privkey(account2_skey, m_secp_context.get());

    params.append(account2_pkey.data(), account2_pkey.size());
    static constexpr uint64_t val = 20;
    params.append(&val, sizeof(val));
    params.append(&m_init_sequence, sizeof(m_init_sequence));

    auto sig_payload = cbdc::buffer();
    sig_payload.append(account2_pkey.data(), account2_pkey.size());
    sig_payload.append(&val, sizeof(val));
    sig_payload.append(&m_init_sequence, sizeof(m_init_sequence));

    auto sha = CSHA256();
    sha.Write(sig_payload.c_ptr(), sig_payload.size());
    auto sighash = cbdc::hash_t();
    sha.Finalize(sighash.data());

    secp256k1_keypair keypair{};
    [[maybe_unused]] auto ret
        = secp256k1_keypair_create(m_secp_context.get(),
                                   &keypair,
                                   m_init_account_skey.data());

    cbdc::signature_t sig{};
    ret = secp256k1_schnorrsig_sign(m_secp_context.get(),
                                    sig.data(),
                                    sighash.data(),
                                    &keypair,
                                    nullptr,
                                    nullptr);
    params.append(sig.data(), sig.size());

    auto exp_from_acc_key = cbdc::buffer();
    exp_from_acc_key.append("account_", 8);
    exp_from_acc_key.append(m_init_account_pkey.data(),
                            m_init_account_pkey.size());
    auto exp_to_acc_key = cbdc::buffer();
    exp_to_acc_key.append("account_", 8);
    exp_to_acc_key.append(account2_pkey.data(), account2_pkey.size());
    auto exp_from_acc_val = cbdc::buffer();
    constexpr uint64_t from_bal = m_init_balance - val;
    auto ser = cbdc::buffer_serializer(exp_from_acc_val);
    ser << from_bal << (m_init_sequence + 1);
    auto exp_to_acc_val = cbdc::buffer();
    auto ser2 = cbdc::buffer_serializer(exp_to_acc_val);
    ser2 << val << m_init_sequence;

    auto exp_ret = cbdc::parsec::agent::return_type(
        {{exp_from_acc_key, exp_from_acc_val},
         {exp_to_acc_key, exp_to_acc_val}});

    auto complete = false;
    auto agent = std::make_shared<cbdc::parsec::agent::impl>(
        m_log,
        m_cfg,
        &cbdc::parsec::agent::runner::factory<
            cbdc::parsec::agent::runner::lua_runner>::create,
        m_broker,
        m_pay_contract_key,
        params,
        [&](cbdc::parsec::agent::interface::exec_return_type res) {
            ASSERT_TRUE(
                std::holds_alternative<cbdc::parsec::agent::return_type>(res));
            ASSERT_EQ(exp_ret,
                      std::get<cbdc::parsec::agent::return_type>(res));
            complete = true;
        },
        cbdc::parsec::agent::runner::lua_runner::initial_lock_type,
        false,
        nullptr,
        nullptr);
    ASSERT_TRUE(agent->exec());
    ASSERT_TRUE(complete);
}
