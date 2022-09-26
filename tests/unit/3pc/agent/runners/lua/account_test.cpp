// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "../../../util.hpp"
#include "3pc/agent/impl.hpp"
#include "3pc/agent/runners/lua/impl.hpp"
#include "3pc/broker/impl.hpp"
#include "3pc/directory/impl.hpp"
#include "3pc/runtime_locking_shard/impl.hpp"
#include "3pc/ticket_machine/impl.hpp"
#include "crypto/sha256.h"
#include "util/common/keys.hpp"
#include "util/serialization/buffer_serializer.hpp"
#include "util/serialization/format.hpp"

#include <gtest/gtest.h>
#include <secp256k1.h>
#include <secp256k1_schnorrsig.h>
#include <thread>

class account_test : public ::testing::Test {
  protected:
    void SetUp() override {
        m_pay_contract_key.append("pay", 3);
        m_pay_contract
            = cbdc::buffer::from_hex(
                  "1b4c7561540019930d0a1a0a04080878560000000000000000000000287"
                  "74001808ac4010008d48b0000058e0001060381030080010000c4000306"
                  "0f0004050f0003040f0002030f0001020f000001cf0000000f000801cf8"
                  "000000f000901cf0001000f000a01cf8001000f000b01cf0002000f000c"
                  "018b0000090b010000c40002030f000e020f000d018b00000c0b0100018"
                  "b0100020b020003c40004020f000f018b0000100b0100008b0100040b02"
                  "000fc40004018b0000030b01000eba000200380100808b0000110301090"
                  "0c40002018b0000020b01000d3a010100380100808b00001103810900c4"
                  "0002018b000002c0007f00b80700808b0000090b010001c40002030f001"
                  "5020f0014018b0000140b010002a2000102ae0002060f0014018b00000d"
                  "0b010002a3000102ae0002070f000d01380000800f8001168b000003950"
                  "00180af0080060f000e018b00000b0b0100008b01000d0b02000e8b0200"
                  "010b0300148b030015c5000700c6000000c700010097048566726f6d048"
                  "3746f048676616c7565048973657175656e636504847369670487737472"
                  "696e670487756e7061636b0492633332206333322049382049382063363"
                  "404906765745f6163636f756e745f6b6579048c6765745f6163636f756e"
                  "74048d7061636b5f6163636f756e7404907570646174655f6163636f756"
                  "e7473048c7369675f7061796c6f6164048d66726f6d5f62616c616e6365"
                  "048966726f6d5f73657104887061796c6f6164048a636865636b5f73696"
                  "704866572726f72049873657175656e6365206e756d62657220746f6f20"
                  "6c6f770495696e73756666696369656e742062616c616e6365048b746f5"
                  "f62616c616e63650487746f5f736571008100000085808d91010003880f"
                  "8000018b00000000010000b50002000f0002018b000002c8000200c7000"
                  "10083048f6163636f756e745f70726566697804896163636f756e745f04"
                  "8c6163636f756e745f6b657981000000808080808080939c0100049d8b0"
                  "0000100010000c40002020f0000018b0000038e0001040b010000c40002"
                  "020f0002018b0000058e0001060b010002c4000202c0007f00b80400808"
                  "b0000058e000109030105008b010002c40003030f0008020f0007018b00"
                  "00070b010008c60003008180ff7f0181ff7fc6000300c70001008b048c6"
                  "163636f756e745f6b657904906765745f6163636f756e745f6b6579048d"
                  "6163636f756e745f64617461048a636f726f7574696e6504867969656c6"
                  "40487737472696e6704846c656e04906163636f756e745f62616c616e63"
                  "6504916163636f756e745f73657175656e63650487756e7061636b04864"
                  "938204938810000008080808080809ea00400098b0b0200008002010044"
                  "0202028b0200018e020502038301008003020000040300c402040210000"
                  "405470201008404906765745f6163636f756e745f6b6579048773747269"
                  "6e6704857061636b0486493820493881000000808080808080a2a906000"
                  "b9413030000520000000f0000060b0300018b0300000004000080040100"
                  "0005020044030501bc810200b80200800b0300018b03000000040300800"
                  "4040000050500440305010b030000480302004703010083048472657404"
                  "8d7061636b5f6163636f756e740081000000808080808080abad0300088"
                  "98b0100008e01030103020100800200000003010080030200c5010500c6"
                  "010000c7010100830487737472696e6704857061636b048a63333220493"
                  "820493881000000808080808080808080")
                  .value();
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
    cbdc::threepc::config m_cfg{};
    std::shared_ptr<cbdc::threepc::runtime_locking_shard::interface> m_shard0{
        std::make_shared<cbdc::threepc::runtime_locking_shard::impl>(m_log)};
    std::shared_ptr<cbdc::threepc::runtime_locking_shard::interface> m_shard1{
        std::make_shared<cbdc::threepc::runtime_locking_shard::impl>(m_log)};
    std::shared_ptr<cbdc::threepc::runtime_locking_shard::interface> m_shard2{
        std::make_shared<cbdc::threepc::runtime_locking_shard::impl>(m_log)};
    std::shared_ptr<cbdc::threepc::runtime_locking_shard::interface> m_shard3{
        std::make_shared<cbdc::threepc::runtime_locking_shard::impl>(m_log)};
    std::shared_ptr<cbdc::threepc::ticket_machine::interface> m_ticketer{
        std::make_shared<cbdc::threepc::ticket_machine::impl>(m_log, 10)};
    std::shared_ptr<cbdc::threepc::directory::interface> m_directory{
        std::make_shared<cbdc::threepc::directory::impl>(4)};
    std::shared_ptr<cbdc::threepc::broker::interface> m_broker{
        std::make_shared<cbdc::threepc::broker::impl>(
            0,
            std::vector<std::shared_ptr<
                cbdc::threepc::runtime_locking_shard::interface>>(
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

    auto exp_ret = cbdc::threepc::agent::return_type(
        {{exp_from_acc_key, exp_from_acc_val},
         {exp_to_acc_key, exp_to_acc_val}});

    auto complete = false;
    auto agent = std::make_shared<cbdc::threepc::agent::impl>(
        m_log,
        m_cfg,
        &cbdc::threepc::agent::runner::factory<
            cbdc::threepc::agent::runner::lua_runner>::create,
        m_broker,
        m_pay_contract_key,
        params,
        [&](cbdc::threepc::agent::interface::exec_return_type res) {
            ASSERT_TRUE(
                std::holds_alternative<cbdc::threepc::agent::return_type>(
                    res));
            ASSERT_EQ(exp_ret,
                      std::get<cbdc::threepc::agent::return_type>(res));
            complete = true;
        },
        cbdc::threepc::agent::runner::lua_runner::initial_lock_type,
        false,
        nullptr,
        nullptr);
    ASSERT_TRUE(agent->exec());
    ASSERT_TRUE(complete);
}
