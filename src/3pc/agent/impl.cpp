// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "impl.hpp"

#include "3pc/agent/runners/evm/format.hpp"
#include "3pc/agent/runners/evm/serialization.hpp"
#include "util/common/variant_overloaded.hpp"
#include "util/serialization/util.hpp"

#include <atomic>

namespace cbdc::threepc::agent {
    impl::impl(std::shared_ptr<logging::log> logger,
               cbdc::threepc::config cfg,
               runner::interface::factory_type runner_factory,
               std::shared_ptr<broker::interface> broker,
               runtime_locking_shard::key_type function,
               parameter_type param,
               exec_callback_type result_callback,
               broker::lock_type initial_lock_type,
               bool dry_run,
               std::shared_ptr<secp256k1_context> secp,
               std::shared_ptr<thread_pool> t_pool,
               std::shared_ptr<cbdc::telemetry> tel)
        : interface(std::move(function),
                    std::move(param),
                    std::move(result_callback)),
          m_log(std::move(logger)),
          m_cfg(std::move(cfg)),
          m_runner_factory(std::move(runner_factory)),
          m_broker(std::move(broker)),
          m_initial_lock_type(dry_run ? broker::lock_type::read
                                      : initial_lock_type),
          m_dry_run(dry_run),
          m_secp(std::move(secp)),
          m_threads(std::move(t_pool)),
          m_tel(std::move(tel)) {}

    auto impl::exec() -> bool {
        std::unique_lock l(m_mut);
        switch(m_state) {
            // In these states we can start again from the beginning
            case state::init:
                [[fallthrough]];
            case state::begin_sent:
            case state::begin_failed:
                break;

            // We already have a ticket number but need to start again
            case state::rollback_complete:
                m_result = std::nullopt;
                m_wounded = false;
                m_restarted = true;
                do_start();
                return true;

            // Re-run commit
            case state::commit_failed:
                [[fallthrough]];
            case state::commit_sent:
                do_commit();
                return true;

            // Re-run rollback with prior error type flag
            case state::rollback_failed:
                [[fallthrough]];
            case state::rollback_sent:
                do_rollback(m_permanent_error);
                return true;

            // Rollback first so we can start fresh
            case state::function_get_sent:
                [[fallthrough]];
            case state::function_get_failed:
            case state::function_failed:
            case state::function_started:
                do_rollback(false);
                return true;

            // Re-run finish
            case state::finish_sent:
                [[fallthrough]];
            case state::finish_failed:
                // Committed but transient error running finish, cannot
                // rollback, need to retry finish
                do_finish();
                return true;

            // End states, cannot re-run exec
            case state::function_get_error:
                [[fallthrough]];
            case state::commit_error:
            case state::function_error:
            case state::finish_complete:
                return true;
        }

        m_result = std::nullopt;
        m_state = state::begin_sent;
        auto start = telemetry::nano_now();
        auto success = m_broker->begin(
            [&, start](broker::interface::begin_return_type res) {
                auto retcode = static_cast<uint8_t>(res.index());
                telemetry_log("broker_begin", retcode, start);
                handle_begin(res);
            });

        if(!success) {
            m_state = state::begin_failed;
            m_log->error("Failed to contact broker to begin");
            m_result = error_code::broker_unreachable;
            // telemetry_log("agent_do_begin", 1, start);
            do_result();
        }

        return true;
    }

    void impl::telemetry_log(const cbdc::telemetry_key& func,
                             uint8_t outcome,
                             int64_t start) {
        telemetry_log(func, cbdc::telemetry_details{}, outcome, start);
    }

    void impl::telemetry_log(const cbdc::telemetry_key& func,
                             const cbdc::telemetry_details& details,
                             uint8_t outcome,
                             int64_t start) {
        if(!m_tel) {
            return;
        }
        if(start == 0) {
            m_log->error("called telemetry_log with start of 0");
            return;
        }
        auto det = cbdc::telemetry_details{
            {cbdc::telemetry_keys::outcome, outcome},
            {cbdc::telemetry_keys::latency, telemetry::nano_now() - start}};
        if(m_ticket_number.has_value()) {
            det.emplace_back(cbdc::telemetry_keys::ticket_number,
                             m_ticket_number.value());
        }
        det.insert(det.begin(), details.begin(), details.end());
        static constexpr cbdc::hash_t empty_hash = cbdc::hash_t{};
        if(!m_tx_id.has_value()) {
            // TODO: make this generic over the runner type
            auto param = get_param();
            auto tx = from_buffer<runner::evm_tx>(param);
            if(tx.has_value()) {
                m_tx_id = cbdc::threepc::agent::runner::tx_id(tx.value());
            } else {
                m_tx_id = empty_hash;
            }
        }
        if(!(m_tx_id == empty_hash)) {
            det.emplace_back(cbdc::telemetry_keys::txid, m_tx_id.value());
        }
        m_tel->log(func, det);
    }

    void impl::handle_begin(broker::interface::begin_return_type res) {
        std::unique_lock l(m_mut);
        if(m_state != state::begin_sent) {
            m_log->warn("handle_begin while not in begin_sent state");
            return;
        }
        std::visit(
            overloaded{[&](const ticket_machine::ticket_number_type& n) {
                           m_ticket_number = n;
                           do_start();
                       },
                       [&](const broker::interface::error_code& /* e */) {
                           m_state = state::begin_failed;
                           m_log->error(
                               "Broker failed to assign a ticket number");
                           m_result = error_code::ticket_number_assignment;
                           do_result();
                       }},
            res);
    }

    void impl::do_start() {
        // auto start = telemetry::nano_now();
        std::unique_lock l(m_mut);
        assert(m_ticket_number.has_value());
        assert(m_state == state::begin_sent
               || m_state == state::rollback_complete);
        m_state = state::function_get_sent;

        if(m_dry_run && get_function().size() == 0) {
            // If this is a dry-run and the function key is empty, the
            // runner will handle retrieving any keys directly.
            // telemetry_log("agent_do_start", 1, start);
            handle_function(broker::value_type());
            return;
        }

        // for one-byte functions, don't resolve but use the one byte and
        // pass it along. This is used in the EVM runner to distinguish
        // between sending a transaction or querying something (account
        // data for instance). Since we don't know the from here for EVM,
        // since it relies on the signature check, we only pass the
        // transaction as m_param and let the runner figure it out.
        if(get_function().size() == 1) {
            // telemetry_log("agent_do_start", 2, start);
            handle_function(broker::value_type(get_function()));
            return;
        }

        m_log->trace("do_start ", get_function().to_hex());

        auto tl_success = m_broker->try_lock(
            m_ticket_number.value(),
            get_function(),
            m_initial_lock_type,
            [this](const broker::interface::try_lock_return_type& lock_res) {
                // telemetry_log("agent_do_start", 0, start);
                handle_function(lock_res);
            });
        if(!tl_success) {
            m_state = state::function_get_failed;
            m_log->error("Failed to contact broker to retrieve "
                         "function code");
            m_result = error_code::broker_unreachable;
            // telemetry_log("agent_do_start", 3, start);
            do_result();
        }
    }

    void impl::handle_try_lock_response(
        const broker::interface::try_lock_callback_type& res_cb,
        broker::interface::try_lock_return_type res) {
        std::unique_lock l(m_mut);
        if(m_state != state::function_started) {
            m_log->error("try_lock response while not in "
                         "function_started state");
            return;
        }
        if(std::holds_alternative<runtime_locking_shard::shard_error>(res)) {
            auto& err = std::get<runtime_locking_shard::shard_error>(res);
            if(err.m_error_code
               == runtime_locking_shard::error_code::wounded) {
                m_wounded = true;
            }
        }
        res_cb(std::move(res));
    }

    auto impl::handle_try_lock_request(
        broker::key_type key,
        broker::lock_type locktype,
        broker::interface::try_lock_callback_type res_cb) -> bool {
        // TODO: permissions for keys
        std::unique_lock l(m_mut);
        assert(m_ticket_number.has_value());
        if(m_state != state::function_started) {
            m_log->warn("handle_try_lock_request while not in "
                        "function_started state");
            return false;
        }

        if(m_dry_run && locktype == broker::lock_type::write) {
            m_log->warn("handle_try_lock_request of type write when "
                        "m_dry_run = true");
            return false;
        }

        auto it = m_requested_locks.find(key);
        if(it == m_requested_locks.end()
           || it->second == broker::lock_type::read) {
            m_requested_locks[key] = locktype;
        }

        if(m_wounded) {
            m_log->debug(
                "Skipping lock request because ticket is already wounded");
            handle_try_lock_response(
                res_cb,
                runtime_locking_shard::shard_error{
                    runtime_locking_shard::error_code::wounded,
                    std::nullopt});
            return true;
        }

        auto start = telemetry::nano_now();
        auto actual_lock_type = m_dry_run ? broker::lock_type::read : locktype;
        auto det = cbdc::telemetry_details{
            {cbdc::telemetry_keys::storagekey, key},
            {cbdc::telemetry_keys::locktype,
             static_cast<uint8_t>(actual_lock_type)}};
        return m_broker->try_lock(
            m_ticket_number.value(),
            std::move(key),
            actual_lock_type,
            [this, det = std::move(det), cb = std::move(res_cb), start](
                broker::interface::try_lock_return_type res) {
                telemetry_log("broker_try_lock",
                              det,
                              static_cast<uint8_t>(res.index()),
                              start);
                handle_try_lock_response(cb, std::move(res));
            });
    }

    void
    impl::handle_function(const broker::interface::try_lock_return_type& res) {
        // auto start = telemetry::nano_now();
        std::unique_lock l(m_mut);
        if(m_state != state::function_get_sent) {
            m_log->warn(
                "handle_function while not in function_get_sent state");
            // telemetry_log("agent_handle_function", 1, start);
            return;
        }
        std::visit(
            overloaded{
                [&](const broker::value_type& v) {
                    /*m_log->trace(this,
                                 "Starting function m_function ",
                                 v.to_hex());*/
                    m_state = state::function_started;
                    auto reacq_locks
                        = std::make_shared<broker::held_locks_set_type>();
                    (*reacq_locks).swap(m_requested_locks);

                    if(reacq_locks->empty()) {
                        do_runner(v);
                        return;
                    }

                    // Re-acquire previously held locks upon retries
                    // immediately
                    m_log->trace("Re-acquiring locks for",
                                 m_ticket_number.value());
                    auto reacquired = std::make_shared<std::atomic<size_t>>();
                    for(auto& it : *reacq_locks) {
                        m_log->trace("Re-acquiring lock on",
                                     it.first.to_hex(),
                                     "type",
                                     static_cast<int>(it.second),
                                     "for",
                                     m_ticket_number.value());
                        auto success = handle_try_lock_request(
                            it.first,
                            it.second,
                            [this, reacquired, v, reacq_locks](
                                const broker::interface::
                                    try_lock_return_type&) {
                                std::unique_lock ll(m_mut);
                                auto reacq = (*reacquired)++;
                                m_log->trace("Re-acquired",
                                             reacq + 1,
                                             "of",
                                             reacq_locks->size(),
                                             "locks for",
                                             m_ticket_number.value());

                                if(reacq + 1 == reacq_locks->size()) {
                                    do_runner(v);
                                }
                            });
                        if(!success) {
                            m_log->error("Try lock request failed for",
                                         m_ticket_number.value());
                            m_state = state::function_get_failed;
                            m_result = error_code::function_retrieval;
                            do_result();
                            return;
                        }
                    }
                },
                [&](broker::interface::error_code /* e */) {
                    m_state = state::function_get_failed;
                    m_log->error("Failed to retrieve function");
                    m_result = error_code::function_retrieval;
                    // telemetry_log("agent_handle_function", 3, start);
                    do_result();
                },
                [&](const runtime_locking_shard::shard_error& e) {
                    if(e.m_error_code
                       == runtime_locking_shard::error_code::wounded) {
                        m_state = state::function_get_failed;
                        m_log->trace("Shard wounded ticket while "
                                     "retrieving function");
                    } else {
                        m_state = state::function_get_error;
                        m_log->error("Shard error retrieving function");
                    }
                    m_result = error_code::function_retrieval;
                    // telemetry_log("agent_handle_function", 4, start);
                    do_result();
                }},
            res);
    }

    void impl::do_runner(broker::value_type v) {
        m_runner = m_runner_factory(
            m_log,
            m_cfg,
            std::move(v),
            get_param(),
            m_dry_run,
            [this](const runner::interface::run_return_type& run_res) {
                // telemetry_log("agent_handle_function",
                //                     0,
                //                     start);
                handle_run(run_res);
            },
            [this](broker::key_type key,
                   broker::lock_type locktype,
                   broker::interface::try_lock_callback_type res_cb) -> bool {
                return handle_try_lock_request(std::move(key),
                                               locktype,
                                               std::move(res_cb));
            },
            m_secp,
            m_restarted ? nullptr : m_threads,
            m_tel,
            m_ticket_number.value());
        auto run_res = m_runner->run();
        if(!run_res) {
            // telemetry_log("agent_handle_function", 2, start);
            m_state = state::function_failed;
            m_log->error("Failed to start contract execution");
            m_result = error_code::function_execution;
            do_result();
        }
    }

    void impl::do_commit() {
        // auto start = telemetry::nano_now();
        std::unique_lock l(m_mut);
        assert(m_state == state::function_started
               || m_state == state::commit_failed
               || m_state == state::commit_sent);
        assert(m_result.has_value());
        assert(m_ticket_number.has_value());
        assert(std::holds_alternative<broker::state_update_type>(
            m_result.value()));
        m_state = state::commit_sent;
        m_log->trace(this,
                     "Agent requesting commit for",
                     m_ticket_number.value());
        auto payload = return_type();
        if(!m_dry_run) {
            payload = std::get<broker::state_update_type>(m_result.value());
        }
        auto start = telemetry::nano_now();
        auto maybe_success = m_broker->commit(
            m_ticket_number.value(),
            payload,
            [this, start](broker::interface::commit_return_type commit_res) {
                constexpr auto success_code = 255;
                uint8_t retcode{success_code};
                auto det = cbdc::telemetry_details{};
                if(commit_res.has_value()) {
                    if(std::holds_alternative<
                           runtime_locking_shard::shard_error>(
                           commit_res.value())) {
                        auto err
                            = std::get<runtime_locking_shard::shard_error>(
                                commit_res.value());
                        static constexpr int shard_error_offset = 64;
                        retcode = static_cast<uint8_t>(
                            shard_error_offset
                            + static_cast<uint8_t>(err.m_error_code));
                        if(err.m_wounded_details.has_value()) {
                            det.emplace_back(
                                cbdc::telemetry_keys::ticket_number2,
                                err.m_wounded_details->m_wounding_ticket);
                            det.emplace_back(
                                cbdc::telemetry_keys::storagekey2,
                                err.m_wounded_details->m_wounding_key);
                        }
                    } else {
                        retcode = static_cast<uint8_t>(commit_res->index());
                    }
                }
                telemetry_log("broker_commit", det, retcode, start);
                handle_commit(commit_res);
            });
        if(!maybe_success) {
            m_state = state::commit_failed;
            m_log->error("Failed to contact broker for commit");
            m_result = error_code::broker_unreachable;
            // telemetry_log("agent_do_commit", 1, start);
            do_result();
        }
    }

    void impl::handle_run(const runner::interface::run_return_type& res) {
        // auto start = telemetry::nano_now();
        std::unique_lock l(m_mut);
        if(m_state != state::function_started) {
            m_log->warn("handle_run while not in function_started state");
            // telemetry_log("agent_handle_run", 1, start);
            return;
        }
        std::visit(
            overloaded{
                [&](runtime_locking_shard::state_update_type states) {
                    m_result = std::move(states);
                    // telemetry_log("agent_handle_run", 0, start);
                    do_commit();
                },
                [&](runner::interface::error_code e) {
                    if(e == runner::interface::error_code::wounded
                       || e == runner::interface::error_code::internal_error) {
                        m_state = state::function_failed;
                    } else {
                        m_state = state::function_error;
                        m_log->error(this,
                                     "function execution failed for",
                                     m_ticket_number.value());
                    }
                    m_result = error_code::function_execution;
                    // telemetry_log("agent_handle_run", 2, start);
                    do_result();
                }},
            res);
        m_log->trace(this,
                     "Agent handle_run complete for",
                     m_ticket_number.value());
    }

    void impl::handle_commit(broker::interface::commit_return_type res) {
        // auto start = telemetry::nano_now();
        std::unique_lock l(m_mut);
        if(m_state != state::commit_sent) {
            m_log->warn(
                this,
                "Agent handle_commit while not in commit_sent state for",
                m_ticket_number.value(),
                "actual state:");
            // telemetry_log("agent_handle_commit", 1, start);
            return;
        }
        if(res.has_value()) {
            std::visit(
                overloaded{
                    [&](broker::interface::error_code /* e */) {
                        m_state = state::commit_failed;
                        m_log->error("Broker error for commit for",
                                     m_ticket_number.value());
                        m_result = error_code::commit_error;
                        // telemetry_log("agent_handle_commit", 2, start);
                        do_result();
                    },
                    [&](const runtime_locking_shard::shard_error& e) {
                        if(e.m_error_code
                           == runtime_locking_shard::error_code::wounded) {
                            m_state = state::commit_failed;
                            m_log->trace(m_ticket_number.value(),
                                         "wounded during commit");
                        } else {
                            m_state = state::commit_error;
                            m_log->error("Shard error for commit for",
                                         m_ticket_number.value());
                        }
                        m_result = error_code::commit_error;
                        // telemetry_log("agent_handle_commit", 3, start);
                        do_result();
                    },
                    [&](runtime_locking_shard::error_code /*e*/) {
                        m_state = state::commit_error;
                        m_log->error("Shard error for commit for",
                                     m_ticket_number.value());
                        m_result = error_code::commit_error;
                        // telemetry_log("agent_handle_commit", 3, start);
                        do_result();
                    }},
                res.value());
        } else {
            m_log->trace(this,
                         "Agent handled commit for",
                         m_ticket_number.value());
            // telemetry_log("agent_handle_commit", 0, start);
            do_finish();
        }
    }

    void impl::do_result() {
        // auto start = telemetry::nano_now();
        std::unique_lock l(m_mut);
        assert(m_result.has_value());
        switch(m_state) {
            // No results should be reported in these states, fatal bugs
            case state::init:
                m_log->fatal("Result reported in initial state");
            case state::begin_sent:
                m_log->fatal("Result reported in begin_sent state");
            case state::function_get_sent:
                m_log->fatal("Result reported in function_get_sent state");
            case state::commit_sent:
                m_log->fatal("Result reported in commit_sent state");
            case state::finish_sent:
                m_log->fatal("Result reported in finish_sent state");
            case state::function_started:
                m_log->fatal("Result reported in function_started state");
            case state::rollback_sent:
                m_log->fatal("Result reported in rollback_sent state");
            case state::rollback_complete:
                if(!std::holds_alternative<error_code>(m_result.value())
                   || std::get<error_code>(m_result.value())
                          != error_code::retry) {
                    m_log->fatal("Result reported in rollback_complete state "
                                 "when result is not retry");
                }
                [[fallthrough]];

            // Failure due to transient problems, should retry
            case state::begin_failed:
                // Couldn't get a ticket number, no need to rollback
                break;

            case state::function_get_failed:
                [[fallthrough]];
            case state::function_failed:
            case state::commit_failed:
                // telemetry_log("agent_do_result", 1, start);
                do_rollback(false);
                return;

            case state::finish_failed:
                // Committed but transient error running finish, cannot
                // rollback, need to retry finish
                [[fallthrough]];
            case state::rollback_failed:
                // Need to retry rollback
                break;

            // Failure due to permanent error, abort completely
            case state::function_get_error:
                [[fallthrough]];
            case state::commit_error:
            case state::function_error:
                // telemetry_log("agent_do_result", 2, start);
                do_rollback(true);
                return;

            // Ran to completion
            case state::finish_complete:
                m_log->debug(this, "Agent finished", m_ticket_number.value());
                break;
        }
        // telemetry_log("agent_do_result", 0, start);
        get_result_callback()(m_result.value());
        m_log->trace(this,
                     "Agent handled result for",
                     m_ticket_number.value());
    }

    void impl::do_finish() {
        // auto start = telemetry::nano_now();
        std::unique_lock l(m_mut);
        assert(m_state == state::commit_sent || m_state == state::finish_failed
               || m_state == state::finish_sent
               || m_state == state::rollback_complete);
        assert(m_ticket_number.has_value());
        m_state = state::finish_sent;
        m_log->trace(this,
                     "Agent requesting finish for",
                     m_ticket_number.value());
        auto start = telemetry::nano_now();
        auto maybe_success = m_broker->finish(
            m_ticket_number.value(),
            [this, start](broker::interface::finish_return_type finish_res) {
                constexpr auto success_code = 255;
                uint8_t retcode{success_code};
                if(finish_res.has_value()) {
                    retcode = static_cast<uint8_t>(finish_res.value());
                }
                telemetry_log("broker_finish", retcode, start);
                handle_finish(finish_res);
            });
        if(!maybe_success) {
            m_state = state::finish_failed;
            m_log->error("Error contacting broker for finish");
            m_result = error_code::broker_unreachable;
            // telemetry_log("agent_do_finish", 1, start);
            do_result();
        }
    }

    void
    impl::handle_finish(broker::interface::finish_return_type finish_res) {
        // auto start = telemetry::nano_now();
        std::unique_lock l(m_mut);
        if(m_state != state::finish_sent) {
            m_log->warn("handle_finish while not in finish_sent state");
            // telemetry_log("agent_handle_finish", 1, start);
            return;
        }
        if(finish_res.has_value()) {
            m_state = state::finish_failed;
            m_log->error("Broker error for finish for",
                         m_ticket_number.value());
            m_result = error_code::finish_error;
            do_result();
        } else {
            m_state = state::finish_complete;
            m_log->trace(this,
                         "Agent handled finish for",
                         m_ticket_number.value());
            do_result();
        }
        // telemetry_log("agent_handle_finish", 0, start);
    }

    void impl::do_rollback(bool finish) {
        // auto start = telemetry::nano_now();
        std::unique_lock l(m_mut);
        assert(m_state == state::commit_failed
               || m_state == state::rollback_sent
               || m_state == state::function_error
               || m_state == state::function_failed
               || m_state == state::commit_error
               || m_state == state::function_get_failed
               || m_state == state::function_get_error
               || m_state == state::function_started
               || m_state == state::rollback_failed);
        assert(m_ticket_number.has_value());
        m_log->trace(this, "Agent rolling back", m_ticket_number.value());
        m_state = state::rollback_sent;
        m_permanent_error = finish;
        auto start = telemetry::nano_now();
        auto maybe_success = m_broker->rollback(
            m_ticket_number.value(),
            [this,
             start](broker::interface::rollback_return_type rollback_res) {
                constexpr auto success_code = 255;
                uint8_t retcode{success_code};
                if(rollback_res.has_value()) {
                    retcode = static_cast<uint8_t>(rollback_res->index());
                }
                telemetry_log("broker_rollback", retcode, start);
                handle_rollback(rollback_res);
            });
        if(!maybe_success) {
            m_state = state::rollback_failed;
            m_log->error("Error contacting broker for rollback");
            m_result = error_code::broker_unreachable;
            // telemetry_log("agent_do_rollback", 1, start);
            do_result();
        }
    }

    void impl::handle_rollback(
        broker::interface::rollback_return_type rollback_res) {
        // auto start = telemetry::nano_now();
        std::unique_lock l(m_mut);
        if(m_state != state::rollback_sent) {
            m_log->warn("handle_rollback while not in rollback_sent state");
            // telemetry_log("agent_handle_rollback", 1, start);
            return;
        }
        if(rollback_res.has_value()) {
            m_state = state::rollback_failed;
            m_result = error_code::rollback_error;
            m_log->error("Broker error rolling back", m_ticket_number.value());
            // telemetry_log("agent_handle_rollback", 2, start);
            do_result();
            return;
        }

        m_state = state::rollback_complete;
        m_log->trace(this, "Agent rolled back", m_ticket_number.value());
        if(m_permanent_error) {
            m_log->trace(this,
                         "Agent finishing due to permanent error",
                         m_ticket_number.value());
            // telemetry_log("agent_handle_rollback", 3, start);
            do_finish();
        } else {
            // Transient error, try again
            m_log->debug(this,
                         "Agent should restart",
                         m_ticket_number.value());
            m_result = error_code::retry;
            // telemetry_log("agent_handle_rollback", 4, start);
            do_result();
        }
    }

    impl::~impl() {
        std::unique_lock l(m_mut);
        if(m_state != state::finish_complete) {
            m_log->fatal(
                this,
                "Agent state wasn't finished at destruction, state was:",
                static_cast<int>(m_state));
        }
    }

    auto impl::get_ticket_number() const
        -> std::optional<ticket_machine::ticket_number_type> {
        std::unique_lock l(m_mut);
        return m_ticket_number;
    }

    auto impl::get_state() const -> state {
        std::unique_lock l(m_mut);
        return m_state;
    }
}
