// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "http_server.hpp"

#include "3pc/agent/runners/evm/format.hpp"
#include "3pc/agent/runners/evm/impl.hpp"
#include "3pc/agent/runners/evm/math.hpp"
#include "3pc/agent/runners/evm/serialization.hpp"
#include "3pc/agent/runners/evm/util.hpp"
#include "impl.hpp"
#include "util/serialization/format.hpp"

#include <cassert>
#include <future>

using namespace cbdc::threepc::agent::runner;

namespace cbdc::threepc::agent::rpc {
    http_server::http_server(std::unique_ptr<server_type> srv,
                             std::shared_ptr<broker::interface> broker,
                             std::shared_ptr<logging::log> log,
                             std::shared_ptr<cbdc::telemetry> tel,
                             const cbdc::threepc::config& cfg)
        : server_interface(std::move(broker),
                           std::move(log),
                           std::move(tel),
                           cfg),
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
        m_log->trace("received request", method);

        if(method == "eth_sendRawTransaction") {
            return handle_send_raw_transaction(params, callback);
        }
        if(method == "eth_getTransactionCount") {
            return handle_get_transaction_count(params, callback);
        }

        m_log->warn("Unknown method", method);
        return false;
    }

    auto http_server::handle_send_raw_transaction(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        auto dry_run = true;
        auto function = cbdc::buffer();
        auto runner_params = cbdc::buffer();
        auto res_cb = std::function<void(interface::exec_return_type)>();

        if(!params.isArray()) {
            m_log->warn("Parameter is not an array");
            return false;
        }

        if(params.empty()) {
            m_log->warn("Not enough parameters");
            return false;
        }

        if(!params[0].isString()) {
            m_log->warn("TX is not a string");
            return false;
        }

        constexpr auto f_type
            = runner::evm_runner_function::execute_transaction;
        function.append(&f_type, sizeof(f_type));
        auto params_str = params[0].asString();
        auto maybe_raw_tx = cbdc::buffer::from_hex(params_str.substr(2));
        if(!maybe_raw_tx.has_value()) {
            m_log->warn("Unable to decode params", params_str);
            return false;
        }
        dry_run = false;

        auto maybe_tx = tx_decode(maybe_raw_tx.value(), m_log);
        if(!maybe_tx.has_value()) {
            m_log->warn("Unable to deserialize transaction");
            return false;
        }
        auto& tx = maybe_tx.value();
        runner_params = make_buffer(*tx);

        res_cb = [callback, tx](interface::exec_return_type res) {
            auto ret = Json::Value();
            if(!std::holds_alternative<return_type>(res)) {
                auto ec = std::get<interface::error_code>(res);
                ret["error"] = static_cast<int>(ec);
                callback(ret);
                return;
            }

            auto txid = cbdc::make_buffer(tx_id(*tx));
            ret["result"] = txid.to_hex();
            callback(ret);
        };

        return exec_tx(dry_run, function, runner_params, res_cb);
    }

    auto http_server::handle_get_transaction_count(
        Json::Value params,
        const server_type::result_callback_type& callback) -> bool {
        auto dry_run = true;
        auto function = cbdc::buffer();
        auto runner_params = cbdc::buffer();
        auto res_cb = std::function<void(interface::exec_return_type)>();

        if(!params.isArray()) {
            m_log->warn("Parameter is not an array");
            return false;
        }

        if(params.empty()) {
            m_log->warn("Not enough parameters");
            return false;
        }

        if(!params[0].isString()) {
            m_log->warn("Address is not a string");
            return false;
        }

        constexpr auto f_type = runner::evm_runner_function::read_account;
        function.append(&f_type, sizeof(f_type));
        auto params_str = params[0].asString();
        auto maybe_runner_params
            = cbdc::buffer::from_hex(params_str.substr(2));
        if(!maybe_runner_params.has_value()) {
            m_log->warn("Unable to decode params", params_str);
            return false;
        }
        runner_params = std::move(maybe_runner_params.value());

        res_cb = [callback, runner_params](interface::exec_return_type res) {
            auto ret = Json::Value();
            if(!std::holds_alternative<return_type>(res)) {
                auto ec = std::get<interface::error_code>(res);
                ret["error"] = static_cast<int>(ec);
                callback(ret);
                return;
            }

            auto& updates = std::get<return_type>(res);
            auto it = updates.find(runner_params);
            assert(it != updates.end());

            auto maybe_acc
                = cbdc::from_buffer<runner::evm_account>(it->second);
            assert(maybe_acc.has_value());

            auto& acc = maybe_acc.value();

            auto tx_count = acc.m_nonce + evmc::uint256be(1);
            ret["result"] = to_hex(tx_count);
            callback(ret);
        };

        return exec_tx(dry_run, function, runner_params, res_cb);
    }

    auto http_server::exec_tx(
        bool dry_run,
        cbdc::buffer function,
        cbdc::buffer runner_params,
        std::function<void(interface::exec_return_type)> res_cb) -> bool {
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
                dry_run,
                m_secp,
                m_threads,
                m_tel);
            {
                std::unique_lock l(m_agents_mut);
                m_agents.emplace(id, agent);
            }
            return agent;
        }();
        return a->exec();
    }
}
