// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.hpp"

#include "parsec/util.hpp"
#include "util/common/config.hpp"
#include "util/common/random_source.hpp"
#include "util/serialization/format.hpp"

#include <future>
#include <secp256k1_schnorrsig.h>

namespace cbdc::parsec {
    account_wallet::account_wallet(std::shared_ptr<logging::log> log,
                                   std::shared_ptr<broker::interface> broker,
                                   std::shared_ptr<agent::rpc::client> agent,
                                   cbdc::buffer pay_contract_key)
        : m_log(std::move(log)),
          m_agent(std::move(agent)),
          m_broker(std::move(broker)),
          m_pay_contract_key(std::move(pay_contract_key)) {
        auto rnd = cbdc::random_source(cbdc::config::random_source);
        m_privkey = rnd.random_hash();
        m_pubkey = cbdc::pubkey_from_privkey(m_privkey, m_secp.get());
        constexpr auto account_prefix = "account_";
        m_account_key.append(account_prefix, std::strlen(account_prefix));
        m_account_key.append(m_pubkey.data(), m_pubkey.size());
    }

    auto account_wallet::init(uint64_t value,
                              const std::function<void(bool)>& result_callback)
        -> bool {
        auto init_account = cbdc::buffer();
        auto ser = cbdc::buffer_serializer(init_account);
        ser << value << m_sequence;
        auto res = put_row(m_broker,
                           m_account_key,
                           init_account,
                           [&, result_callback, value](bool ret) {
                               if(ret) {
                                   m_balance = value;
                               }
                               result_callback(ret);
                           });
        return res;
    }

    auto account_wallet::pay(pubkey_t to,
                             uint64_t amount,
                             const std::function<void(bool)>& result_callback)
        -> bool {
        if(amount > m_balance) {
            return false;
        }
        auto params = make_pay_params(to, amount);
        return execute_params(params, false, result_callback);
    }

    auto account_wallet::get_pubkey() const -> pubkey_t {
        return m_pubkey;
    }

    auto account_wallet::update_balance(
        const std::function<void(bool)>& result_callback) -> bool {
        auto params = make_pay_params(pubkey_t{}, 0);
        return execute_params(params, false, result_callback);
    }

    auto account_wallet::make_pay_params(pubkey_t to, uint64_t amount) const
        -> cbdc::buffer {
        auto params = cbdc::buffer();
        params.append(m_pubkey.data(), m_pubkey.size());
        params.append(to.data(), to.size());
        params.append(&amount, sizeof(amount));
        params.append(&m_sequence, sizeof(m_sequence));

        auto sig_payload = cbdc::buffer();
        sig_payload.append(to.data(), to.size());
        sig_payload.append(&amount, sizeof(amount));
        sig_payload.append(&m_sequence, sizeof(m_sequence));

        auto sha = CSHA256();
        sha.Write(sig_payload.c_ptr(), sig_payload.size());
        auto sighash = cbdc::hash_t();
        sha.Finalize(sighash.data());

        secp256k1_keypair keypair{};
        [[maybe_unused]] auto ret = secp256k1_keypair_create(m_secp.get(),
                                                             &keypair,
                                                             m_privkey.data());

        cbdc::signature_t sig{};
        ret = secp256k1_schnorrsig_sign32(m_secp.get(),
                                          sig.data(),
                                          sighash.data(),
                                          &keypair,
                                          nullptr);
        params.append(sig.data(), sig.size());
        return params;
    }

    auto account_wallet::execute_params(
        cbdc::buffer params,
        bool dry_run,
        const std::function<void(bool)>& result_callback) -> bool {
        auto send_success = m_agent->exec(
            m_pay_contract_key,
            std::move(params),
            dry_run,
            [&, result_callback](agent::interface::exec_return_type res) {
                auto success = std::holds_alternative<agent::return_type>(res);
                if(success) {
                    auto updates = std::get<agent::return_type>(res);
                    auto it = updates.find(m_account_key);
                    assert(it != updates.end());
                    auto deser = cbdc::buffer_serializer(it->second);
                    deser >> m_balance >> m_sequence;
                }
                result_callback(success);
            });
        return send_success;
    }

    auto account_wallet::get_balance() const -> uint64_t {
        return m_balance;
    }
}
