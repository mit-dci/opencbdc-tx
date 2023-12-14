// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.hpp"

#include "parsec/util.hpp"
#include "util/common/config.hpp"
#include "util/common/random_source.hpp"
#include "util/serialization/format.hpp"

#include <Python.h>
#include <future>
#include <secp256k1_schnorrsig.h>

namespace cbdc::parsec {
    pybench_wallet::pybench_wallet(std::shared_ptr<logging::log> log,
                                   std::shared_ptr<broker::interface> broker,
                                   std::shared_ptr<agent::rpc::client> agent,
                                   cbdc::buffer pay_contract_key,
                                   const std::string& pubkey)
        : m_log(std::move(log)),
          m_agent(std::move(agent)),
          m_broker(std::move(broker)),
          m_pay_contract_key(std::move(pay_contract_key)) {
        m_account_key = runtime_locking_shard::key_type();
        m_account_key.append(pubkey.c_str(), pubkey.size() + 1);
    }

    auto pybench_wallet::init(uint64_t value,
                              const std::function<void(bool)>& result_callback)
        -> bool {
        auto init_val = runtime_locking_shard::value_type();
        init_val.append(std::to_string(value).c_str(),
                        std::to_string(value).length() + 1);
        auto res = put_row(m_broker,
                           m_account_key,
                           init_val,
                           [&, result_callback, value](bool ret) {
                               if(ret) {
                                   m_balance = value;
                               }
                               result_callback(ret);
                           });
        return res;
    }

    auto pybench_wallet::get_balance() const -> uint64_t {
        return m_balance;
    }

    auto pybench_wallet::get_account_key() const
        -> runtime_locking_shard::key_type {
        return m_account_key;
    }

    auto pybench_wallet::make_pay_params(runtime_locking_shard::key_type to,
                                         uint64_t amount) const
        -> cbdc::parsec::pybuffer::pyBuffer {
        auto params = cbdc::parsec::pybuffer::pyBuffer();

        // User defined input parameters
        params.appendNumeric<uint64_t>(amount);
        params.endSection();

        // Input parameters stored in shards
        params.append(m_account_key.data(), m_account_key.size());
        params.append(to.data(), to.size());
        params.endSection();

        // Ouptut parameters to be stoed in shards
        params.append(m_account_key.data(), m_account_key.size());
        params.append(to.data(), to.size());
        params.endSection();

        return params;
    }

    auto pybench_wallet::execute_params(
        cbdc::parsec::pybuffer::pyBuffer params,
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
                    m_balance = std::stoi(it->second.c_str());
                    m_log->trace("Balance of:",
                                 m_account_key.c_str(),
                                 ":",
                                 m_balance);
                }
                result_callback(success);
            });
        return send_success;
    }

    auto
    pybench_wallet::pay(runtime_locking_shard::key_type to,
                        uint64_t amount,
                        const std::function<void(bool)>& result_callback) // 2
        -> bool {
        if(amount > m_balance) {
            return false;
        }
        auto params = make_pay_params(std::move(to), amount);
        return execute_params(params, false, result_callback);
    }
}
