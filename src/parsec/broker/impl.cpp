// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "impl.hpp"

#include "util/common/variant_overloaded.hpp"

#include <cassert>

namespace cbdc::parsec::broker {
    impl::impl(
        runtime_locking_shard::broker_id_type broker_id,
        std::vector<std::shared_ptr<runtime_locking_shard::interface>> shards,
        std::shared_ptr<ticket_machine::interface> ticketer,
        std::shared_ptr<directory::interface> directory,
        std::shared_ptr<logging::log> logger)
        : m_broker_id(broker_id),
          m_shards(std::move(shards)),
          m_ticketer(std::move(ticketer)),
          m_directory(std::move(directory)),
          m_log(std::move(logger)) {}

    auto impl::begin(begin_callback_type result_callback) -> bool {
        if(!m_ticketer->get_ticket_number(
               [this, result_callback](
                   std::optional<parsec::ticket_machine::interface::
                                     get_ticket_number_return_type> res) {
                   handle_ticket_number(result_callback, res);
               })) {
            m_log->error("Failed to request a ticket number");
            result_callback(error_code::ticket_machine_unreachable);
        }

        return true;
    }

    void impl::handle_ticket_number(
        begin_callback_type result_callback,
        std::optional<
            parsec::ticket_machine::interface::get_ticket_number_return_type>
            res) {
        if(!res.has_value()) {
            result_callback(error_code::ticket_number_assignment);
            return;
        }
        std::visit(overloaded{[&](const parsec::ticket_machine::interface::
                                      ticket_number_range_type& n) {
                                  {
                                      std::unique_lock l(m_mut);
                                      if(m_highest_ticket < n.second) {
                                          m_highest_ticket = n.second;
                                      }
                                      m_tickets.emplace(
                                          n.first,
                                          std::make_shared<state>(
                                              state{ticket_state::begun, {}}));
                                  }
                                  result_callback(n.first);
                              },
                              [&](const parsec::ticket_machine::interface::
                                      error_code& /* e */) {
                                  result_callback(
                                      error_code::ticket_number_assignment);
                              }},
                   res.value());
    }

    auto impl::highest_ticket() -> ticket_number_type {
        return m_highest_ticket;
    }

    void impl::log_tickets() {
        m_log->trace("Logging tickets");
        for(auto i : m_tickets) {
            auto i_state = i.second->m_state;
            switch(i_state) {
                case ticket_state::begun:
                    m_log->trace("Ticket Log", i.first, "begun");
                    break;
                case ticket_state::prepared:
                    m_log->trace("Ticket Log", i.first, "prepared");
                    break;
                case ticket_state::committed:
                    m_log->trace("Ticket Log", i.first, "committed");
                    break;
                case ticket_state::aborted:
                    m_log->trace("Ticket Log", i.first, "aborted");
                    break;
                default:
                    m_log->warn("Ticket Log", i.first, "Cannot resolve state");
                    break;
            }
        }
    }

    void impl::handle_lock(
        ticket_number_type ticket_number,
        key_type key,
        uint64_t shard_idx,
        const try_lock_callback_type& result_callback,
        const parsec::runtime_locking_shard::interface::try_lock_return_type&
            res) {
        auto result = std::visit(
            overloaded{
                [&](parsec::runtime_locking_shard::value_type v)
                    -> try_lock_return_type {
                    std::unique_lock l(m_mut);
                    auto it = m_tickets.find(ticket_number);
                    if(it == m_tickets.end()) {
                        return error_code::unknown_ticket;
                    }

                    auto t_state = it->second;
                    auto& s_state = t_state->m_shard_states[shard_idx];
                    auto k_it = s_state.m_key_states.find(key);
                    if(k_it == s_state.m_key_states.end()) {
                        m_log->error("Shard state not found for key");
                        return error_code::invalid_shard_state;
                    }

                    if(k_it->second.m_key_state != key_state::locking) {
                        m_log->error("Shard state not locking");
                        return error_code::invalid_shard_state;
                    }

                    k_it->second.m_key_state = key_state::locked;
                    k_it->second.m_value = v;

                    m_log->trace(this, "Broker locked key for", ticket_number);
                    // m_log->trace(this, "Key:", key.c_str());
                    // m_log->trace(this, "Returning:", v.c_str());
                    return v;
                },
                [&, key](parsec::runtime_locking_shard::shard_error e)
                    -> try_lock_return_type {
                    if(e.m_wounded_details.has_value()) {
                        m_log->trace(this,
                                     e.m_wounded_details->m_wounding_ticket,
                                     "wounded ticket",
                                     ticket_number);
                    }
                    m_log->trace(this,
                                 "Shard error",
                                 static_cast<int>(e.m_error_code),
                                 "locking key",
                                 key.to_hex(),
                                 "for",
                                 ticket_number);
                    return e;
                }},
            res);
        result_callback(result);
    }

    auto impl::try_lock(ticket_number_type ticket_number,
                        key_type key,
                        lock_type locktype,
                        try_lock_callback_type result_callback) -> bool {
        auto maybe_error = [&]() -> std::optional<error_code> {
            std::unique_lock l(m_mut);
            auto it = m_tickets.find(ticket_number);
            if(it == m_tickets.end()) {
                return error_code::unknown_ticket;
            }

            auto t_state = it->second;
            switch(t_state->m_state) {
                case ticket_state::begun:
                    break;
                case ticket_state::prepared:
                    return error_code::prepared;
                case ticket_state::committed:
                    return error_code::committed;
                case ticket_state::aborted:
                    t_state->m_state = ticket_state::begun;
                    t_state->m_shard_states.clear();
                    m_log->trace(this, "broker restarting", ticket_number);
                    break;
            }

            if(!m_directory->key_location(
                   key,
                   [=](std::optional<
                       parsec::directory::interface::key_location_return_type>
                           res) {
                       handle_find_key(ticket_number,
                                       key,
                                       locktype,
                                       result_callback,
                                       res);
                   })) {
                m_log->error("Failed to make key location directory request");
                return error_code::directory_unreachable;
            }

            return std::nullopt;
        }();

        if(maybe_error.has_value()) {
            result_callback(maybe_error.value());
        }

        return true;
    }

    void impl::handle_prepare(
        const commit_callback_type& commit_cb,
        ticket_number_type ticket_number,
        uint64_t shard_idx,
        parsec::runtime_locking_shard::interface::prepare_return_type res) {
        auto maybe_error = [&]() -> std::optional<commit_return_type> {
            std::unique_lock ll(m_mut);
            auto itt = m_tickets.find(ticket_number);
            if(itt == m_tickets.end()) {
                return error_code::unknown_ticket;
            }

            auto ts = itt->second;
            switch(ts->m_state) {
                case ticket_state::begun:
                    break;
                case ticket_state::prepared:
                    return error_code::prepared;
                case ticket_state::committed:
                    return error_code::committed;
                case ticket_state::aborted:
                    return error_code::aborted;
            }

            return do_handle_prepare(commit_cb,
                                     ticket_number,
                                     ts,
                                     shard_idx,
                                     res);
        }();

        m_log->trace(this, "Broker handled prepare for", ticket_number);

        if(maybe_error.has_value()) {
            m_log->trace(this,
                         "Broker calling prepare callback with error for",
                         ticket_number);
            commit_cb(maybe_error.value());
        }
    }

    auto impl::do_handle_prepare(
        const commit_callback_type& commit_cb,
        ticket_number_type ticket_number,
        const std::shared_ptr<state>& ts,
        uint64_t shard_idx,
        const parsec::runtime_locking_shard::interface::prepare_return_type&
            res) -> std::optional<commit_return_type> {
        auto& ss = ts->m_shard_states[shard_idx].m_state;
        if(ss != shard_state_type::preparing) {
            m_log->trace(this,
                         "Shard",
                         shard_idx,
                         "not in preparing state for",
                         ticket_number);
            return std::nullopt;
        }

        if(res.has_value()) {
            if(res.value().m_error_code
               != runtime_locking_shard::error_code::wounded) {
                m_log->error("Shard error with prepare for", ticket_number);
            } else {
                m_log->trace("Shard",
                             shard_idx,
                             "wounded ticket",
                             ticket_number);
                for(auto& [sidx, s] : ts->m_shard_states) {
                    if(s.m_state == shard_state_type::wounded) {
                        return std::nullopt;
                    }
                }
                ss = shard_state_type::wounded;
            }
            return res.value();
        }

        m_log->trace(this,
                     "Broker setting shard",
                     shard_idx,
                     "to prepared for",
                     ticket_number);
        ss = shard_state_type::prepared;

        for(auto& shard : ts->m_shard_states) {
            if(shard.second.m_state != shard_state_type::prepared) {
                return std::nullopt;
            }
        }

        ts->m_state = ticket_state::prepared;

        auto maybe_error = do_commit(commit_cb, ticket_number, ts);
        if(maybe_error.has_value()) {
            return maybe_error.value();
        }
        return std::nullopt;
    }

    auto impl::do_commit(const commit_callback_type& commit_cb,
                         ticket_number_type ticket_number,
                         const std::shared_ptr<state>& ts)
        -> std::optional<error_code> {
        for(auto& shard : ts->m_shard_states) {
            if(ts->m_state == ticket_state::aborted) {
                m_log->trace("Broker aborted during commit for",
                             ticket_number);
                break;
            }
            if(shard.second.m_state == shard_state_type::committed) {
                continue;
            }
            shard.second.m_state = shard_state_type::committing;
            auto sidx = shard.first;
            if(!m_shards[sidx]->commit(
                   ticket_number,
                   [=](const parsec::runtime_locking_shard::interface::
                           commit_return_type& comm_res) {
                       handle_commit(commit_cb, ticket_number, sidx, comm_res);
                   })) {
                m_log->error("Failed to make commit shard request");
                return error_code::shard_unreachable;
            }
        }
        return std::nullopt;
    }

    void impl::handle_commit(
        const commit_callback_type& commit_cb,
        ticket_number_type ticket_number,
        uint64_t shard_idx,
        parsec::runtime_locking_shard::interface::commit_return_type res) {
        auto callback = false;
        auto maybe_error = [&]() -> std::optional<error_code> {
            std::unique_lock lll(m_mut);
            auto ittt = m_tickets.find(ticket_number);
            if(ittt == m_tickets.end()) {
                return error_code::unknown_ticket;
            }

            auto tss = ittt->second;
            switch(tss->m_state) {
                case ticket_state::begun:
                    return error_code::not_prepared;
                case ticket_state::prepared:
                    break;
                case ticket_state::committed:
                    return error_code::committed;
                case ticket_state::aborted:
                    return error_code::aborted;
            }

            if(tss->m_shard_states[shard_idx].m_state
               != shard_state_type::committing) {
                m_log->error("Commit result when shard not committing");
                return error_code::invalid_shard_state;
            }

            if(res.has_value()) {
                m_log->error("Error committing on shard");
                return error_code::commit_error;
            }

            tss->m_shard_states[shard_idx].m_state
                = shard_state_type::committed;

            for(auto& shard : tss->m_shard_states) {
                if(shard.second.m_state != shard_state_type::committed) {
                    return std::nullopt;
                }
            }

            tss->m_state = ticket_state::committed;
            callback = true;

            m_log->trace(this, "Broker handled commit for", ticket_number);

            return std::nullopt;
        }();

        if(maybe_error.has_value()) {
            m_log->trace(this,
                         "Broker calling commit callback with error for",
                         ticket_number);
            commit_cb(maybe_error.value());
        } else if(callback) {
            m_log->trace(this,
                         "Broker calling commit callback from handle_commit "
                         "with success for",
                         ticket_number);
            commit_cb(std::nullopt);
        }
    }

    auto impl::commit(ticket_number_type ticket_number,
                      state_update_type state_updates,
                      commit_callback_type result_callback) -> bool {
        m_log->trace(this, "Broker got commit request for", ticket_number);
        auto maybe_error = [&]() -> std::optional<error_code> {
            std::unique_lock l(m_mut);
            auto it = m_tickets.find(ticket_number);
            if(it == m_tickets.end()) {
                return error_code::unknown_ticket;
            }

            auto t_state = it->second;
            switch(t_state->m_state) {
                case ticket_state::begun:
                    [[fallthrough]];
                case ticket_state::prepared:
                    break;
                case ticket_state::committed:
                    return error_code::committed;
                case ticket_state::aborted:
                    return error_code::aborted;
            }

            for(auto& shard : t_state->m_shard_states) {
                for(auto& key : shard.second.m_key_states) {
                    if(key.second.m_key_state == key_state::locking) {
                        m_log->error("Cannot commit, still waiting for locks");
                        return error_code::waiting_for_locks;
                    }
                }
            }

            if(t_state->m_state == ticket_state::prepared) {
                return do_commit(result_callback, ticket_number, t_state);
            }
            return do_prepare(result_callback,
                              ticket_number,
                              t_state,
                              state_updates);
        }();

        if(maybe_error.has_value()) {
            m_log->trace(
                this,
                "Broker calling commit callback with error from commit for",
                ticket_number);
            result_callback(maybe_error.value());
        }

        return true;
    }

    auto impl::do_prepare(const commit_callback_type& result_callback,
                          ticket_number_type ticket_number,
                          const std::shared_ptr<state>& t_state,
                          const state_update_type& state_updates)
        -> std::optional<error_code> {
        for(auto& shard : t_state->m_shard_states) {
            // Shard states might get nuked in this loop if there's a
            // rollback Before iterating, make sure we're still good to
            // do more prepares Only nuke shard states on restart
            if(t_state->m_state == ticket_state::aborted) {
                m_log->trace("Broker aborted during prepare for",
                             ticket_number);
                break;
            }
            if(shard.second.m_state == shard_state_type::prepared) {
                continue;
            }
            shard.second.m_state = shard_state_type::preparing;
            auto shard_updates = state_update_type();
            for(const auto& update : state_updates) {
                if(shard.second.m_key_states.find(update.first)
                   != shard.second.m_key_states.end()) {
                    shard_updates.emplace(update);
                }
            }
            if(!m_shards[shard.first]->prepare(
                   ticket_number,
                   m_broker_id,
                   std::move(shard_updates),
                   [this,
                    result_callback,
                    ticket_number,
                    shard_idx
                    = shard.first](const parsec::runtime_locking_shard::
                                       interface::prepare_return_type& res) {
                       handle_prepare(result_callback,
                                      ticket_number,
                                      shard_idx,
                                      res);
                   })) {
                m_log->error("Failed to make prepare shard request");
                return error_code::shard_unreachable;
            }
        }
        return std::nullopt;
    }

    auto impl::finish(ticket_number_type ticket_number,
                      finish_callback_type result_callback) -> bool {
        auto done = false;
        auto maybe_error = [&]() -> std::optional<error_code> {
            std::unique_lock l(m_mut);
            auto it = m_tickets.find(ticket_number);
            if(it == m_tickets.end()) {
                m_log->trace(this,
                             "Broker failing finish: [Unknown ticket] for ",
                             ticket_number);
                return error_code::unknown_ticket;
            }

            auto t_state = it->second;
            switch(t_state->m_state) {
                case ticket_state::begun:
                    m_log->trace(this,
                                 "Broker failing finish: [State = Begun] for ",
                                 ticket_number);
                    return error_code::begun;
                case ticket_state::prepared:
                    m_log->trace(
                        this,
                        "Broker failing finish: [State = Prepared] for ",
                        ticket_number);
                    return error_code::prepared;
                case ticket_state::committed:
                    break;
                case ticket_state::aborted:
                    // Ticket already rolled back. Just delete the ticket.
                    m_tickets.erase(it);
                    done = true;
                    return std::nullopt;
            }

            for(auto& shard : t_state->m_shard_states) {
                m_log->trace(this,
                             "Broker requesting finish on",
                             shard.first,
                             "for ticket",
                             ticket_number);
                if(shard.second.m_state == shard_state_type::finished) {
                    m_log->trace(this,
                                 "Broker skipping finish on",
                                 shard.first,
                                 "for ticket",
                                 ticket_number,
                                 " already finished");
                    continue;
                }
                auto sidx = shard.first;
                assert(sidx < m_shards.size());
                shard.second.m_state = shard_state_type::finishing;
                if(!m_shards[sidx]->finish(
                       ticket_number,
                       [=](const parsec::runtime_locking_shard::interface::
                               finish_return_type& res) {
                           handle_finish(result_callback,
                                         ticket_number,
                                         sidx,
                                         res);
                       })) {
                    m_log->error("Failed to make finish shard request");
                    return error_code::shard_unreachable;
                }
            }

            return std::nullopt;
        }();

        if(maybe_error.has_value()) {
            result_callback(maybe_error.value());
        } else if(done) {
            result_callback(std::nullopt);
        }

        return true;
    }

    auto impl::rollback(ticket_number_type ticket_number,
                        rollback_callback_type result_callback) -> bool {
        m_log->trace(this, "Broker got rollback request for", ticket_number);
        auto callback = false;
        auto maybe_error = [&]() -> std::optional<error_code> {
            std::unique_lock l(m_mut);
            auto it = m_tickets.find(ticket_number);
            if(it == m_tickets.end()) {
                return error_code::unknown_ticket;
            }

            auto t_state = it->second;
            switch(t_state->m_state) {
                case ticket_state::begun:
                    break;
                case ticket_state::prepared:
                    return error_code::prepared;
                case ticket_state::committed:
                    return error_code::committed;
                case ticket_state::aborted:
                    return error_code::aborted;
            }

            if(t_state->m_shard_states.empty()) {
                callback = true;
                t_state->m_state = ticket_state::aborted;
                return std::nullopt;
            }

            for(auto& shard : t_state->m_shard_states) {
                m_log->trace(this,
                             "Broker requesting rollback on",
                             shard.first,
                             "for ticket",
                             ticket_number);
                if(shard.second.m_state == shard_state_type::rolled_back) {
                    m_log->trace(this,
                                 "Broker skipping rollback on",
                                 shard.first,
                                 "for ticket",
                                 ticket_number,
                                 " already rolled back");
                    continue;
                }
                auto sidx = shard.first;
                assert(sidx < m_shards.size());
                shard.second.m_state = shard_state_type::rolling_back;
                if(!m_shards[sidx]->rollback(
                       ticket_number,
                       [=](const parsec::runtime_locking_shard::interface::
                               rollback_return_type& res) {
                           handle_rollback(result_callback,
                                           ticket_number,
                                           sidx,
                                           res);
                       })) {
                    m_log->error("Failed to make rollback shard request");
                    return error_code::shard_unreachable;
                }
            }

            return std::nullopt;
        }();

        m_log->trace(this,
                     "Broker initiated rollback request for",
                     ticket_number);

        if(maybe_error.has_value()) {
            result_callback(maybe_error.value());
        } else if(callback) {
            result_callback(std::nullopt);
        }

        m_log->trace(this,
                     "Broker handled rollback request for",
                     ticket_number);

        return true;
    }

    void impl::handle_rollback(
        const rollback_callback_type& result_callback,
        ticket_number_type ticket_number,
        uint64_t shard_idx,
        parsec::runtime_locking_shard::interface::rollback_return_type res) {
        auto callback = false;
        auto maybe_error = [&]() -> std::optional<error_code> {
            std::unique_lock lll(m_mut);
            auto ittt = m_tickets.find(ticket_number);
            if(ittt == m_tickets.end()) {
                return error_code::unknown_ticket;
            }

            auto tss = ittt->second;
            switch(tss->m_state) {
                case ticket_state::begun:
                    break;
                case ticket_state::prepared:
                    return error_code::prepared;
                case ticket_state::committed:
                    return error_code::committed;
                case ticket_state::aborted:
                    return error_code::aborted;
            }

            if(tss->m_shard_states[shard_idx].m_state
               != shard_state_type::rolling_back) {
                m_log->error(
                    "Rollback response for",
                    ticket_number,
                    "when shard",
                    shard_idx,
                    "not in rolling back state. Actual state:",
                    static_cast<int>(tss->m_shard_states[shard_idx].m_state));
                return error_code::invalid_shard_state;
            }

            if(res.has_value()) {
                m_log->error("Shard rollback error");
                return error_code::rollback_error;
            }

            auto& s_state = tss->m_shard_states[shard_idx];

            s_state.m_state = shard_state_type::rolled_back;
            s_state.m_key_states.clear();
            m_log->trace(this,
                         "Shard",
                         shard_idx,
                         "rolled back for",
                         ticket_number);

            for(auto& shard : tss->m_shard_states) {
                if(shard.second.m_state != shard_state_type::rolled_back) {
                    m_log->trace(this,
                                 "Shard",
                                 shard.first,
                                 "not yet rolled back for",
                                 ticket_number,
                                 ". Shard state:",
                                 static_cast<int>(shard.second.m_state));
                    return std::nullopt;
                }
            }

            m_log->trace(this, "All shards rolled back for", ticket_number);

            tss->m_state = ticket_state::aborted;
            callback = true;
            return std::nullopt;
        }();

        if(maybe_error.has_value()) {
            result_callback(maybe_error.value());
        } else if(callback) {
            result_callback(std::nullopt);
        }
    }

    void impl::handle_find_key(
        ticket_number_type ticket_number,
        key_type key,
        lock_type locktype,
        try_lock_callback_type result_callback,
        std::optional<parsec::directory::interface::key_location_return_type>
            res) {
        auto maybe_error = [&]() -> std::optional<try_lock_return_type> {
            std::unique_lock l(m_mut);
            assert(res < m_shards.size());
            auto ticket = m_tickets.find(ticket_number);
            if(ticket == m_tickets.end()) {
                m_log->error("Unknown ticket number");
                return error_code::unknown_ticket;
            }

            auto tss = ticket->second;
            switch(tss->m_state) {
                case ticket_state::begun:
                    break;
                case ticket_state::prepared:
                    return error_code::prepared;
                case ticket_state::committed:
                    return error_code::committed;
                case ticket_state::aborted:
                    return error_code::aborted;
            }

            if(!res.has_value()) {
                return error_code::directory_unreachable;
            }

            auto shard_idx = res.value();
            auto& ss = tss->m_shard_states[shard_idx];
            auto first_lock = ss.m_key_states.empty();
            auto it = ss.m_key_states.find(key);
            if(it != ss.m_key_states.end()
               && it->second.m_key_state == key_state::locked
               && it->second.m_locktype >= locktype) {
                assert(it->second.m_value.has_value());
                return it->second.m_value.value();
            }

            auto& ks = ss.m_key_states[key];

            ks.m_key_state = key_state::locking;
            ks.m_locktype = locktype;

            if(!m_shards[shard_idx]->try_lock(
                   ticket_number,
                   m_broker_id,
                   key,
                   locktype,
                   first_lock,
                   [=](const parsec::runtime_locking_shard::interface::
                           try_lock_return_type& lock_res) {
                       handle_lock(ticket_number,
                                   key,
                                   shard_idx,
                                   result_callback,
                                   lock_res);
                   })) {
                m_log->error("Failed to make try_lock shard request");
                return error_code::shard_unreachable;
            }

            return std::nullopt;
        }();

        if(maybe_error.has_value()) {
            // if(locktype == lock_type::read) {
            //     // set ticket state to aborted
            //     auto ticket = m_tickets.find(ticket_number);
            //     if(ticket != m_tickets.end()) {
            //         ticket->second->m_state = ticket_state::aborted;
            //     }
            // }
            result_callback(maybe_error.value());
        }
        // // if read, set ticket state to committed
        // if(locktype == lock_type::read) {
        //     // set ticket state to aborted
        //     auto ticket = m_tickets.find(ticket_number);
        //     if(ticket != m_tickets.end()) { // this check is probably not
        //     required
        //         ticket->second->m_state = ticket_state::committed;
        //     }
        // }
    }

    void impl::handle_finish(
        const finish_callback_type& result_callback,
        ticket_number_type ticket_number,
        uint64_t shard_idx,
        parsec::runtime_locking_shard::interface::finish_return_type res) {
        auto callback = false;
        auto maybe_error = [&]() -> std::optional<error_code> {
            std::unique_lock lll(m_mut);
            auto ittt = m_tickets.find(ticket_number);
            if(ittt == m_tickets.end()) {
                return error_code::unknown_ticket;
            }

            auto tss = ittt->second;
            switch(tss->m_state) {
                case ticket_state::begun:
                    return error_code::begun;
                case ticket_state::prepared:
                    return error_code::prepared;
                case ticket_state::committed:
                    break;
                case ticket_state::aborted:
                    return error_code::aborted;
            }

            if(tss->m_shard_states[shard_idx].m_state
               != shard_state_type::finishing) {
                m_log->error(
                    "Finish response for",
                    ticket_number,
                    "when shard",
                    shard_idx,
                    "not in finishing state. Actual state:",
                    static_cast<int>(tss->m_shard_states[shard_idx].m_state));
                return error_code::invalid_shard_state;
            }

            if(res.has_value()) {
                m_log->error("Shard finish error");
                return error_code::finish_error;
            }

            tss->m_shard_states[shard_idx].m_state
                = shard_state_type::finished;
            m_log->trace(this,
                         "Shard",
                         shard_idx,
                         "finished for",
                         ticket_number);

            for(auto& shard : tss->m_shard_states) {
                if(shard.second.m_state != shard_state_type::finished) {
                    m_log->trace(this,
                                 "Shard",
                                 shard.first,
                                 "not yet finished for",
                                 ticket_number,
                                 ". Shard state:",
                                 static_cast<int>(shard.second.m_state));
                    return std::nullopt;
                }
            }

            m_log->trace(this, "All shards finished for", ticket_number);

            m_tickets.erase(ittt);

            callback = true;
            return std::nullopt;
        }();

        if(maybe_error.has_value()) {
            result_callback(maybe_error.value());
        } else if(callback) {
            result_callback(std::nullopt);
        }
    }

    auto impl::recover(recover_callback_type result_callback) -> bool {
        // Do not allow recovery when tickets are in-flight
        auto maybe_tickets = [&]() {
            std::unique_lock l(m_mut);
            return !m_tickets.empty();
        }();
        if(maybe_tickets) {
            return false;
        }
        for(uint64_t i = 0; i < m_shards.size(); i++) {
            auto& s = m_shards[i];
            auto success = s->get_tickets(
                m_broker_id,
                [&, result_callback, i](
                    const parsec::runtime_locking_shard::interface::
                        get_tickets_return_type& res) {
                    handle_get_tickets(result_callback, i, res);
                });
            if(!success) {
                return false;
            }
        }
        return true;
    }

    void
    impl::handle_get_tickets(const recover_callback_type& result_callback,
                             uint64_t shard_idx,
                             const parsec::runtime_locking_shard::interface::
                                 get_tickets_return_type& res) {
        auto done = false;
        auto maybe_error = std::visit(
            overloaded{[&](const runtime_locking_shard::interface::
                               get_tickets_success_type& tickets)
                           -> std::optional<error_code> {
                           std::unique_lock l(m_mut);
                           m_recovery_tickets.emplace(shard_idx, tickets);
                           if(m_recovery_tickets.size() != m_shards.size()) {
                               return std::nullopt;
                           }
                           for(auto& [s, ts] : m_recovery_tickets) {
                               for(auto& [ticket_number, t_state] : ts) {
                                   auto& ticket = m_tickets[ticket_number];
                                   if(!ticket) {
                                       ticket = std::make_shared<state>();
                                   }
                                   switch(t_state) {
                                       case runtime_locking_shard::
                                           ticket_state::begun:
                                           ticket->m_shard_states[s].m_state
                                               = shard_state_type::begun;
                                           break;
                                       case runtime_locking_shard::
                                           ticket_state::committed:
                                           ticket->m_shard_states[s].m_state
                                               = shard_state_type::committed;
                                           break;
                                       case runtime_locking_shard::
                                           ticket_state::prepared:
                                           ticket->m_shard_states[s].m_state
                                               = shard_state_type::prepared;
                                           break;
                                       case runtime_locking_shard::
                                           ticket_state::wounded:
                                           ticket->m_shard_states[s].m_state
                                               = shard_state_type::wounded;
                                           break;
                                   }
                               }
                           }
                           if(m_tickets.empty()) {
                               done = true;
                               return std::nullopt;
                           }
                           m_recovery_tickets.clear();
                           return do_recovery(result_callback);
                       },
                       [&](const runtime_locking_shard::error_code& /* e */)
                           -> std::optional<error_code> {
                           return error_code::get_tickets_error;
                       }},
            res);
        if(maybe_error.has_value()) {
            result_callback(maybe_error.value());
        } else if(done) {
            result_callback(std::nullopt);
        }

        m_log->trace(this, "Broker handled get_tickets for shard", shard_idx);
    }

    auto impl::do_recovery(const recover_callback_type& result_callback)
        -> std::optional<error_code> {
        for(auto [ticket_number, ticket] : m_tickets) {
            size_t committed{};
            for(auto& [sidx, t_state] : ticket->m_shard_states) {
                switch(t_state.m_state) {
                    case shard_state_type::begun:
                    case shard_state_type::prepared:
                    case shard_state_type::wounded:
                        break;
                    case shard_state_type::committed:
                        committed++;
                        break;
                    default:
                        m_log->fatal(this,
                                     "Found invalid shard "
                                     "state during recovery");
                }
            }
            if(committed == ticket->m_shard_states.size()) {
                ticket->m_state = ticket_state::committed;
                auto success = finish(
                    ticket_number,
                    [&, result_callback](finish_return_type fin_res) {
                        handle_recovery_finish(result_callback, fin_res);
                    });
                if(!success) {
                    return error_code::shard_unreachable;
                }
            } else if(committed > 0) {
                ticket->m_state = ticket_state::prepared;
                auto success = commit(
                    ticket_number,
                    {},
                    [&, result_callback, tn = ticket_number](
                        const commit_return_type& comm_res) {
                        handle_recovery_commit(result_callback, tn, comm_res);
                    });
                if(!success) {
                    return error_code::shard_unreachable;
                }
            } else {
                ticket->m_state = ticket_state::begun;
                auto success
                    = rollback(ticket_number,
                               [&, result_callback, tn = ticket_number](
                                   rollback_return_type roll_res) {
                                   handle_recovery_rollback(result_callback,
                                                            tn,
                                                            roll_res);
                               });
                if(!success) {
                    return error_code::shard_unreachable;
                }
            }
        }
        return std::nullopt;
    }

    void
    impl::handle_recovery_commit(const recover_callback_type& result_callback,
                                 ticket_number_type ticket_number,
                                 const commit_return_type& res) {
        if(res.has_value()) {
            result_callback(error_code::commit_error);
            return;
        }

        auto success
            = finish(ticket_number,
                     [&, result_callback](finish_return_type fin_res) {
                         handle_recovery_finish(result_callback, fin_res);
                     });
        if(!success) {
            result_callback(error_code::shard_unreachable);
        }
    }

    void
    impl::handle_recovery_finish(const recover_callback_type& result_callback,
                                 finish_return_type res) {
        if(res.has_value()) {
            result_callback(error_code::finish_error);
            return;
        }
        auto done = [&]() {
            std::unique_lock l(m_mut);
            return m_tickets.empty();
        }();
        if(done) {
            result_callback(std::nullopt);
        }
    }

    void impl::handle_recovery_rollback(
        const recover_callback_type& result_callback,
        ticket_number_type ticket_number,
        rollback_return_type res) {
        if(res.has_value()) {
            result_callback(error_code::rollback_error);
            return;
        }
        auto success
            = finish(ticket_number,
                     [&, result_callback](finish_return_type fin_res) {
                         handle_recovery_finish(result_callback, fin_res);
                     });
        if(!success) {
            result_callback(error_code::shard_unreachable);
        }
    }
}
