// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "impl.hpp"

#include "format.hpp"
#include "host.hpp"
#include "math.hpp"
#include "serialization.hpp"
#include "signature.hpp"
#include "util.hpp"
#include "util/serialization/format.hpp"

#include <future>

namespace cbdc::threepc::agent::runner {
    evm_runner::evm_runner(std::shared_ptr<logging::log> logger,
                           const cbdc::threepc::config& cfg,
                           runtime_locking_shard::value_type function,
                           parameter_type param,
                           bool dry_run,
                           run_callback_type result_callback,
                           try_lock_callback_type try_lock_callback,
                           std::shared_ptr<secp256k1_context> secp,
                           std::shared_ptr<thread_pool> t_pool,
                           ticket_number_type ticket_number)
        : interface(std::move(logger),
                    cfg,
                    std::move(function),
                    std::move(param),
                    dry_run,
                    std::move(result_callback),
                    std::move(try_lock_callback),
                    std::move(secp),
                    std::move(t_pool),
                    ticket_number) {}

    evm_runner::~evm_runner() {
        for(auto& t : m_evm_threads) {
            if(t.joinable()) {
                t.join();
            }
        }
    }

    void evm_runner::do_run() {
        if(m_function.size() != 1) {
            m_log->error("EVM runner expects 1 byte in m_function, got ",
                         m_function.size());
            m_result_callback(error_code::function_load);
            return;
        }

        static constexpr uint8_t invalid_function = 255;
        uint8_t f = invalid_function;
        std::memcpy(&f, m_function.data(), sizeof(uint8_t));
        if(f > static_cast<uint8_t>(
               evm_runner_function::get_transaction_receipt)) {
            m_log->error("Unknown EVM runner function ", f);
            m_result_callback(error_code::function_load);
            return;
        }

        bool success{false};
        switch(evm_runner_function(f)) {
            case evm_runner_function::execute_transaction:
                success = run_execute_real_transaction();
                break;
            case evm_runner_function::read_account:
                success = run_get_account();
                break;
            case evm_runner_function::dryrun_transaction:
                success = run_execute_dryrun_transaction();
                break;
            case evm_runner_function::read_account_code:
                success = run_get_account_code();
                break;
            case evm_runner_function::get_transaction:
                success = run_get_transaction();
                break;
            case evm_runner_function::get_transaction_receipt:
                success = run_get_transaction_receipt();
                break;
            default:
                m_result_callback(error_code::function_load);
                break;
        }

        if(!success) {
            m_result_callback(error_code::internal_error);
        }
    }

    auto evm_runner::run_get_account() -> bool {
        auto success = m_try_lock_callback(
            m_param,
            broker::lock_type::read,
            [this](const broker::interface::try_lock_return_type& res) {
                if(!std::holds_alternative<broker::value_type>(res)) {
                    m_log->error("Failed to read account from shards");
                    m_result_callback(error_code::function_load);
                    return;
                }
                auto v = std::get<broker::value_type>(res);
                auto ret = runtime_locking_shard::state_update_type();
                ret[m_param] = v;
                m_result_callback(ret);
            });

        return success;
    }

    auto evm_runner::run_get_transaction_receipt() -> bool {
        auto success = m_try_lock_callback(
            m_param,
            broker::lock_type::read,
            [this](const broker::interface::try_lock_return_type& res) {
                if(!std::holds_alternative<broker::value_type>(res)) {
                    m_log->error(
                        "Failed to read transaction receipt from shards");
                    m_result_callback(error_code::function_load);
                    return;
                }
                auto v = std::get<broker::value_type>(res);
                auto ret = runtime_locking_shard::state_update_type();
                ret[m_param] = v;
                m_result_callback(ret);
            });

        return success;
    }

    auto evm_runner::run_get_transaction() -> bool {
        auto success = m_try_lock_callback(
            m_param,
            broker::lock_type::read,
            [this](const broker::interface::try_lock_return_type& res) {
                if(!std::holds_alternative<broker::value_type>(res)) {
                    m_log->error(
                        "Failed to read transaction receipt from shards");
                    m_result_callback(error_code::function_load);
                    return;
                }
                auto v = std::get<broker::value_type>(res);
                auto ret = runtime_locking_shard::state_update_type();

                m_log->trace("Read transaction receipt: ", v.to_hex());

                auto maybe_receipt = cbdc::from_buffer<evm_tx_receipt>(v);
                if(!maybe_receipt.has_value()) {
                    m_log->error("Failed to deserialize transaction receipt");
                    m_result_callback(error_code::function_load);
                    return;
                }
                ret[m_param] = make_buffer(maybe_receipt.value().m_tx);
                m_result_callback(ret);
            });

        return success;
    }

    auto evm_runner::run_get_account_code() -> bool {
        auto addr = evmc::address();
        std::memcpy(addr.bytes, m_param.data(), m_param.size());
        auto key = make_buffer(code_key{addr});
        auto success = m_try_lock_callback(
            key,
            broker::lock_type::read,
            [this](const broker::interface::try_lock_return_type& res) {
                if(!std::holds_alternative<broker::value_type>(res)) {
                    m_log->error("Failed to read account from shards");
                    m_result_callback(error_code::function_load);
                    return;
                }
                auto v = std::get<broker::value_type>(res);
                auto ret = runtime_locking_shard::state_update_type();
                ret[m_param] = v;
                m_result_callback(ret);
            });

        return success;
    }

    auto evm_runner::run_execute_real_transaction() -> bool {
        auto maybe_tx = cbdc::from_buffer<evm_tx>(m_param);
        if(!maybe_tx.has_value()) {
            m_log->error("Unable to deserialize transaction");
            m_result_callback(error_code::function_load);
            return true;
        }
        m_tx = std::move(maybe_tx.value());

        auto maybe_from = check_signature(m_tx, m_secp);
        if(!maybe_from.has_value()) {
            m_log->error("Transaction signature is invalid");
            m_result_callback(error_code::exec_error);
            return true;
        }
        auto from = maybe_from.value();
        return run_execute_transaction(from, false);
    }

    auto evm_runner::run_execute_dryrun_transaction() -> bool {
        auto maybe_tx = cbdc::from_buffer<evm_dryrun_tx>(m_param);
        if(!maybe_tx.has_value()) {
            m_log->error("Unable to deserialize transaction");
            m_result_callback(error_code::function_load);
            return true;
        }
        auto& dryrun_tx = maybe_tx.value();
        m_tx = std::move(dryrun_tx.m_tx);

        return run_execute_transaction(dryrun_tx.m_from, true);
    }

    auto evm_runner::check_base_gas(bool dry_run) const
        -> std::pair<evmc::uint256be, bool> {
        constexpr auto base_gas = evmc::uint256be(21000);
        constexpr auto creation_gas = evmc::uint256be(32000);

        auto min_gas = base_gas;
        if(!m_tx.m_to.has_value()) {
            min_gas = min_gas + creation_gas;
        }

        return std::make_pair(min_gas,
                              !(m_tx.m_gas_limit < min_gas && !dry_run));
    }

    auto evm_runner::make_message(const evmc::address& from, bool dry_run)
        -> std::pair<evmc_message, bool> {
        auto msg = evmc_message();

        auto [min_gas, enough_gas] = check_base_gas(dry_run);
        if(!enough_gas) {
            return std::make_pair(msg, false);
        }

        // Note that input_data is a const reference to the input buffer. The
        // buffer itself must remain in scope while msg is being used. Wrap tx
        // in a shared_ptr and provide it to the thread using msg.
        msg.input_data = m_tx.m_input.data();
        msg.input_size = m_tx.m_input.size();
        msg.depth = 0;

        // Determine transaction type
        if(!m_tx.m_to.has_value()) {
            // Create contract transaction
            msg.kind = EVMC_CREATE;
        } else {
            // Send transaction
            msg.kind = EVMC_CALL;
            msg.recipient = m_tx.m_to.value();
        }

        msg.sender = from;
        msg.value = m_tx.m_value;
        if(dry_run) {
            msg.gas = std::numeric_limits<int64_t>::max();
        } else {
            msg.gas
                = static_cast<int64_t>(to_uint64(m_tx.m_gas_limit - min_gas));
        }
        return std::make_pair(msg, true);
    }

    auto evm_runner::make_tx_context(const evmc::address& from,
                                     bool dry_run) const -> evmc_tx_context {
        auto tx_ctx = evmc_tx_context();
        // TODO: consider setting block height to the TX ticket number
        tx_ctx.block_number = 1;
        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp
            = std::chrono::time_point_cast<std::chrono::seconds>(now);
        tx_ctx.block_timestamp = timestamp.time_since_epoch().count();
        if(!dry_run) {
            tx_ctx.tx_origin = from;
            tx_ctx.tx_gas_price = m_tx.m_gas_price;
            tx_ctx.block_gas_limit
                = static_cast<int64_t>(to_uint64(m_tx.m_gas_limit));
        } else {
            tx_ctx.block_gas_limit = std::numeric_limits<int64_t>::max();
        }
        return tx_ctx;
    }

    auto evm_runner::run_execute_transaction(const evmc::address& from,
                                             bool dry_run) -> bool {
        auto tx_ctx = make_tx_context(from, dry_run);

        m_host = std::make_unique<evm_host>(m_log,
                                            m_try_lock_callback,
                                            tx_ctx,
                                            m_tx,
                                            dry_run,
                                            m_ticket_number);

        auto [msg, enough_gas] = make_message(from, dry_run);
        if(!enough_gas) {
            m_log->trace("TX does not have enough base gas");
            m_result_callback(error_code::exec_error);
            return true;
        }
        m_msg = msg;

        if(!dry_run) {
            m_log->trace(m_ticket_number,
                         "reading from account [",
                         to_hex(from),
                         "]");
            auto addr_key = cbdc::make_buffer(from);
            auto r = m_try_lock_callback(
                addr_key,
                broker::lock_type::write,
                [this](const broker::interface::try_lock_return_type& res) {
                    m_log->trace(m_ticket_number, "read from account");
                    handle_lock_from_account(res);
                });
            if(!r) {
                m_log->error(
                    "Failed to send try_lock request for from account");
                m_result_callback(error_code::internal_error);
                return false;
            }
        } else {
            schedule_exec();
        }

        return true;
    }

    void evm_runner::exec() {
        m_log->trace(this, "Started evm_runner exec");
        auto result = m_host->call(m_msg);
        if(result.status_code < 0) {
            m_log->error("Internal error running EVM contract",
                         evmc::to_string(result.status_code));
            m_result_callback(error_code::internal_error);
        } else if(m_host->should_retry()) {
            m_log->trace("Contract was wounded");
            m_result_callback(error_code::wounded);
        } else {
            if(result.status_code == EVMC_REVERT) {
                m_log->trace("Contract reverted");
                m_host->revert();
            }

            m_log->trace("Result status: ", result.status_code);

            auto gas_used = m_msg.gas - result.gas_left;
            m_host->finalize(result.gas_left, gas_used);
            auto state_updates = m_host->get_state_updates();
            m_result_callback(state_updates);
            auto out_buf = cbdc::buffer();
            out_buf.append(result.output_data, result.output_size);
            m_log->trace("EVM output data:", out_buf.to_hex());
        }
    }

    void evm_runner::handle_lock_from_account(
        const broker::interface::try_lock_return_type& res) {
        if(!std::holds_alternative<broker::value_type>(res)) {
            m_log->debug("Failed to read account from shards");
            m_result_callback(error_code::wounded);
            return;
        }
        auto v = std::get<broker::value_type>(res);
        auto from_acc = evm_account();
        if(v.size() > 0) {
            auto maybe_from_acc = cbdc::from_buffer<evm_account>(v);
            if(maybe_from_acc.has_value()) {
                from_acc = maybe_from_acc.value();
            }
        }

        auto exp_nonce = from_acc.m_nonce + evmc::uint256be(1);
        if(exp_nonce != m_tx.m_nonce) {
            m_log->error(m_ticket_number,
                         "TX has incorrect nonce for from account",
                         to_hex(m_tx.m_nonce),
                         "vs",
                         to_hex(exp_nonce));
            m_result_callback(error_code::exec_error);
            return;
        }

        // TODO: Priority fees for V2 transactions
        auto total_gas_cost = m_tx.m_gas_limit * m_tx.m_gas_price;
        auto required_funds = m_tx.m_value + total_gas_cost;

        if(from_acc.m_balance < required_funds) {
            m_log->error("From account has insufficient funds to cover gas "
                         "and tx value",
                         to_hex(from_acc.m_balance),
                         "vs",
                         to_hex(required_funds));
            m_result_callback(error_code::exec_error);
            return;
        }

        // Deduct gas
        from_acc.m_balance = from_acc.m_balance - total_gas_cost;
        // Increment nonce
        from_acc.m_nonce = from_acc.m_nonce + evmc::uint256be(1);
        m_host->insert_account(m_msg.sender, from_acc);

        const auto txid_key = make_buffer(tx_id(m_tx));

        m_log->trace(m_ticket_number, "locking TXID", txid_key.to_hex());

        // Lock TXID key to store receipt later
        auto maybe_sent = m_try_lock_callback(
            txid_key,
            broker::lock_type::write,
            [this](const broker::interface::try_lock_return_type& r) {
                if(!std::holds_alternative<broker::value_type>(r)) {
                    m_log->debug("Failed to lock key for TX receipt");
                    m_result_callback(error_code::wounded);
                    return;
                }
                m_log->trace(m_ticket_number, "locked TXID key");
                schedule_exec();
            });
        if(!maybe_sent) {
            m_log->error("Failed to send try_lock request for TX receipt");
            m_result_callback(error_code::internal_error);
            return;
        }
    }

    void evm_runner::schedule_exec() {
        auto fn = [this]() {
            exec();
        };
        schedule(fn);
    }

    void evm_runner::schedule(const std::function<void()>& fn) {
        if(m_threads) {
            m_threads->push(fn);
            return;
        }
        m_evm_threads.emplace_back(fn);
    }

    void evm_runner::schedule_run() {
        auto fn = [this]() {
            do_run();
        };
        schedule(fn);
    }

    auto evm_runner::run() -> bool {
        schedule_run();
        return true;
    }
}
