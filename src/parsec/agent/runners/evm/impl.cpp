// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "impl.hpp"

#include "crypto/sha256.h"
#include "format.hpp"
#include "host.hpp"
#include "math.hpp"
#include "serialization.hpp"
#include "signature.hpp"
#include "util.hpp"
#include "util/serialization/format.hpp"

#include <future>

namespace cbdc::parsec::agent::runner {
    evm_runner::evm_runner(std::shared_ptr<logging::log> logger,
                           const cbdc::parsec::config& cfg,
                           runtime_locking_shard::value_type function,
                           parameter_type param,
                           bool is_readonly_run,
                           run_callback_type result_callback,
                           try_lock_callback_type try_lock_callback,
                           std::shared_ptr<secp256k1_context> secp,
                           std::shared_ptr<thread_pool> t_pool,
                           ticket_number_type ticket_number)
        : interface(std::move(logger),
                    cfg,
                    std::move(function),
                    std::move(param),
                    is_readonly_run,
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
        if(f
           > static_cast<uint8_t>(evm_runner_function::read_account_storage)) {
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
            case evm_runner_function::get_block_number:
                success = run_get_block_number();
                break;
            case evm_runner_function::get_block:
                success = run_get_block();
                break;
            case evm_runner_function::get_logs:
                success = run_get_logs();
                break;
            case evm_runner_function::read_account_storage:
                success = run_get_account(); // m_param contains the right key
                                             // already
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
        return m_try_lock_callback(
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
    }

    auto evm_runner::run_get_block_number() -> bool {
        return m_try_lock_callback(
            m_param,
            broker::lock_type::read,
            [this](const broker::interface::try_lock_return_type&) {
                auto ret = runtime_locking_shard::state_update_type();
                ret[m_param]
                    = cbdc::make_buffer(evmc::uint256be(m_ticket_number));
                m_result_callback(ret);
            });
    }

    auto evm_runner::make_pretend_block(interface::ticket_number_type tn)
        -> evm_pretend_block {
        evm_pretend_block blk;
        blk.m_ticket_number = tn;
        blk.m_transactions = {};
        return blk;
    }

    auto evm_runner::run_get_block() -> bool {
        // m_param contains the raw serialized block number - need to decrypt
        // that first and then hash it to get to the key the
        // block(ticket)number to txid mapping is stored under in the shard.

        auto maybe_tn = cbdc::from_buffer<evmc::uint256be>(m_param);
        if(!maybe_tn) {
            return false;
        }
        auto tn = to_uint64(maybe_tn.value());
        auto tn_key = m_host->ticket_number_key(tn);

        auto success = m_try_lock_callback(
            tn_key,
            broker::lock_type::read,
            [this, tn](const broker::interface::try_lock_return_type& res) {
                if(!std::holds_alternative<broker::value_type>(res)) {
                    auto ret = runtime_locking_shard::state_update_type();
                    auto blk = make_pretend_block(tn);
                    ret[m_param] = make_buffer(blk);
                    m_result_callback(ret);
                    return;
                }

                auto v = std::get<broker::value_type>(res);
                auto maybe_txid = from_buffer<cbdc::hash_t>(v);
                if(!maybe_txid) {
                    auto ret = runtime_locking_shard::state_update_type();
                    auto blk = make_pretend_block(tn);
                    ret[m_param] = make_buffer(blk);
                    m_result_callback(ret);
                    return;
                }

                lock_tx_receipt(v, tn);
            });

        return success;
    }

    void evm_runner::lock_tx_receipt(const broker::value_type& value,
                                     const ticket_number_type& ticket_number) {
        auto cb = [this, ticket_number](
                      const broker::interface::try_lock_return_type& res) {
            if(!std::holds_alternative<broker::value_type>(res)) {
                m_log->error("Ticket number had TXID, but TX not found");
                m_result_callback(error_code::function_load);
                return;
            }

            auto v = std::get<broker::value_type>(res);
            auto maybe_tx_receipt = from_buffer<evm_tx_receipt>(v);
            if(!maybe_tx_receipt) {
                m_log->error("Ticket number had TXID, but TX "
                             "receipt could not be deserialized");
                m_result_callback(error_code::function_load);
                return;
            }

            auto ret = runtime_locking_shard::state_update_type();
            auto blk = make_pretend_block(ticket_number);
            blk.m_transactions.push_back(maybe_tx_receipt.value());
            ret[m_param] = make_buffer(blk);
            m_result_callback(ret);
            return;
        };

        if(!m_try_lock_callback(value, broker::lock_type::read, cb)) {
            m_log->error("Could not send request for TX data");
            m_result_callback(error_code::function_load);
        }
    }

    auto evm_runner::run_get_logs() -> bool {
        m_log->info(m_ticket_number, "run_get_logs started");
        // m_param contains the serialized evm_log_query
        auto maybe_qry = cbdc::from_buffer<evm_log_query>(m_param);
        if(!maybe_qry) {
            return false;
        }
        auto qry = maybe_qry.value();

        // First, determine the keys to query for log existence
        auto keys = std::vector<cbdc::buffer>();
        for(auto blk = qry.m_from_block; blk <= qry.m_to_block; blk++) {
            for(auto& it : qry.m_addresses) {
                keys.push_back(m_host->log_index_key(it, blk));
            }
        }

        m_log->info(m_ticket_number,
                    "getting",
                    keys.size(),
                    "keys from shards");

        auto log_indexes_mut = std::make_shared<std::mutex>();
        auto log_indexes = std::make_shared<std::vector<evm_log_index>>();
        auto acquired = std::make_shared<std::atomic<size_t>>();
        for(auto& key : keys) {
            auto success = m_try_lock_callback(
                key,
                broker::lock_type::read,
                [this,
                 acquired,
                 log_indexes,
                 log_indexes_mut,
                 key_count = keys.size(),
                 qry](const broker::interface::try_lock_return_type& res) {
                    handle_get_logs_try_lock_response(qry,
                                                      log_indexes,
                                                      acquired,
                                                      log_indexes_mut,
                                                      key_count,
                                                      res);
                });
            if(!success) {
                m_log->error("Unable to lock logs index key");
                m_result_callback(error_code::internal_error);
                return false;
            }
        }

        return true;
    }

    void evm_runner::handle_get_logs_try_lock_response(
        const evm_log_query& qry,
        const std::shared_ptr<std::vector<evm_log_index>>& log_indexes,
        const std::shared_ptr<std::atomic<size_t>>& acquired,
        const std::shared_ptr<std::mutex>& log_indexes_mut,
        size_t key_count,
        const broker::interface::try_lock_return_type& res) {
        if(!std::holds_alternative<broker::value_type>(res)) {
            m_log->error("Unable to read log key");
            m_result_callback(error_code::function_load);
            return;
        }

        m_log->info(m_ticket_number, "got value from shard");

        auto v = std::get<broker::value_type>(res);
        auto maybe_logs = cbdc::from_buffer<evm_log_index>(v);
        if(maybe_logs) {
            // Found potentially relevant logs, add
            std::unique_lock<std::mutex> lck(*log_indexes_mut);
            log_indexes->push_back(maybe_logs.value());
        }
        if(++(*acquired) == key_count) {
            handle_complete_get_logs(qry, log_indexes_mut, log_indexes);
        }
    }

    void evm_runner::handle_complete_get_logs(
        const evm_log_query& qry,
        const std::shared_ptr<std::mutex>& log_indexes_mut,
        const std::shared_ptr<std::vector<evm_log_index>>& log_indexes) {
        m_log->info(m_ticket_number,
                    "completed all queries, filtering",
                    log_indexes->size(),
                    "logs");

        // Scanned them all - finish
        std::unique_lock<std::mutex> lck(*log_indexes_mut);

        // Filter the final logs by topics
        auto final_logs = std::vector<evm_log_index>();
        for(auto& log_idx : *log_indexes) {
            auto match = false;
            for(auto& log : log_idx.m_logs) {
                for(auto& have_topic : log.m_topics) {
                    for(const auto& want_topic : qry.m_topics) {
                        if(have_topic == want_topic) {
                            match = true;
                            break;
                        }
                    }
                    if(match) {
                        break;
                    }
                }
            }
            if(match) {
                final_logs.push_back(log_idx);
            }
        }
        lck.unlock();

        m_log->info(m_ticket_number,
                    "returning",
                    final_logs.size(),
                    "filtered log indexes");

        auto ret = runtime_locking_shard::state_update_type();
        ret[m_param] = make_buffer(final_logs);
        m_result_callback(ret);
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
        return start_execute_transaction(from, false);
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

        return start_execute_transaction(dryrun_tx.m_from, true);
    }

    auto evm_runner::check_base_gas(const evm_tx& evmtx, bool is_readonly_run)
        -> std::pair<evmc::uint256be, bool> {
        constexpr auto base_gas = evmc::uint256be(21000);
        constexpr auto creation_gas = evmc::uint256be(32000);

        auto min_gas = base_gas;
        if(!evmtx.m_to.has_value()) {
            min_gas = min_gas + creation_gas;
        }

        return std::make_pair(
            min_gas,
            !(evmtx.m_gas_limit < min_gas && !is_readonly_run));
    }

    auto evm_runner::make_message(const evmc::address& from,
                                  const evm_tx& evmtx,
                                  bool is_readonly_run)
        -> std::pair<evmc_message, bool> {
        auto msg = evmc_message();

        auto [min_gas, enough_gas] = check_base_gas(evmtx, is_readonly_run);
        if(!enough_gas) {
            return std::make_pair(msg, false);
        }

        // Note that input_data is a const reference to the input buffer. The
        // buffer itself must remain in scope while msg is being used. Wrap tx
        // in a shared_ptr and provide it to the thread using msg.
        msg.input_data = evmtx.m_input.data();
        msg.input_size = evmtx.m_input.size();
        msg.depth = 0;

        // Determine transaction type
        if(!evmtx.m_to.has_value()) {
            // Create contract transaction
            msg.kind = EVMC_CREATE;
        } else {
            // Send transaction
            msg.kind = EVMC_CALL;
            msg.recipient = evmtx.m_to.value();
        }

        msg.sender = from;
        msg.value = evmtx.m_value;
        if(is_readonly_run) {
            msg.gas = std::numeric_limits<int64_t>::max();
        } else {
            msg.gas
                = static_cast<int64_t>(to_uint64(evmtx.m_gas_limit - min_gas));
        }
        return std::make_pair(msg, true);
    }

    auto evm_runner::make_tx_context(const evmc::address& from,
                                     const evm_tx& evmtx,
                                     bool is_readonly_run) -> evmc_tx_context {
        auto tx_ctx = evmc_tx_context();
        // TODO: consider setting block height to the TX ticket number
        tx_ctx.block_number = 1;
        auto now = std::chrono::system_clock::now();
        auto timestamp
            = std::chrono::time_point_cast<std::chrono::seconds>(now);
        tx_ctx.block_timestamp = timestamp.time_since_epoch().count();
        if(!is_readonly_run) {
            tx_ctx.tx_origin = from;
            tx_ctx.tx_gas_price = evmtx.m_gas_price;
            tx_ctx.block_gas_limit
                = static_cast<int64_t>(to_uint64(evmtx.m_gas_limit));
        } else {
            tx_ctx.block_gas_limit = std::numeric_limits<int64_t>::max();
        }
        return tx_ctx;
    }

    auto evm_runner::start_execute_transaction(const evmc::address& from,
                                               bool is_readonly_run) -> bool {
        auto tx_ctx = make_tx_context(from, m_tx, is_readonly_run);

        m_host = std::make_unique<evm_host>(m_log,
                                            m_try_lock_callback,
                                            tx_ctx,
                                            m_tx,
                                            is_readonly_run,
                                            m_ticket_number);

        auto [msg, enough_gas] = make_message(from, m_tx, is_readonly_run);
        if(!enough_gas) {
            m_log->trace("TX does not have enough base gas");
            m_result_callback(error_code::exec_error);
            return true;
        }
        m_msg = msg;

        if(!is_readonly_run) {
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
                    handle_lockfromaccount_and_continue_exec(res);
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

            auto out_buf = cbdc::buffer();
            out_buf.append(result.output_data, result.output_size);
            m_log->trace("EVM output data:", out_buf.to_hex());

            m_log->trace("Result status: ", result.status_code);

            // finalize_fn() makes the final set of state updates and invokes
            // the main result callback with the full set of accumulated state
            // updates
            auto finalize_fn = [this, gas_left = result.gas_left]() {
                auto gas_used = m_msg.gas - gas_left;
                m_host->finalize(gas_left, gas_used);
                auto state_updates = m_host->get_state_updates();
                m_result_callback(state_updates);
            };

            const auto log_index_keys = m_host->get_log_index_keys();
            if(log_index_keys.empty()) {
                finalize_fn();
            } else {
                lock_index_keys_and_finalize(log_index_keys, finalize_fn);
            }
        }
    }

    void evm_runner::lock_index_keys_and_finalize(
        const std::vector<cbdc::buffer>& keys,
        const std::function<void()>& finalize_fn) {
        auto acquired = std::make_shared<std::atomic<size_t>>();
        for(const auto& key : keys) {
            auto success = m_try_lock_callback(
                key,
                broker::lock_type::write,
                [acquired, finalize_fn, key_count = keys.size()](
                    const broker::interface::try_lock_return_type&) {
                    const bool is_last_key = ++(*acquired) == key_count;
                    if(is_last_key) {
                        finalize_fn();
                    }
                });
            if(!success) {
                m_log->error("Unable to lock logs index key");
                m_result_callback(error_code::internal_error);
                return;
            }
        }
    }

    void evm_runner::handle_lockfromaccount_and_continue_exec(
        const broker::interface::try_lock_return_type& res) {
        if(!std::holds_alternative<broker::value_type>(res)) {
            m_log->debug("Failed to read account from shards");
            m_result_callback(error_code::wounded);
            return;
        }
        auto v = std::get<broker::value_type>(res);
        auto from_acc = evm_account();

        // TODO: Start at zero?
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
                lock_ticket_number_key_and_continue_exec();
            });
        if(!maybe_sent) {
            m_log->error("Failed to send try_lock request for TX receipt");
            m_result_callback(error_code::internal_error);
            return;
        }
    }

    void evm_runner::lock_ticket_number_key_and_continue_exec() {
        auto maybe_sent = m_try_lock_callback(
            m_host->ticket_number_key(),
            broker::lock_type::write,
            [this](const broker::interface::try_lock_return_type& r) {
                if(!std::holds_alternative<broker::value_type>(r)) {
                    m_log->debug("Failed to lock key for ticket_number");
                    m_result_callback(error_code::wounded);
                    return;
                }
                m_log->trace(m_ticket_number, "locked ticket_number key");
                schedule_exec();
            });
        if(!maybe_sent) {
            m_log->error(
                "Failed to send try_lock request for ticket_number key");
            m_result_callback(error_code::internal_error);
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
