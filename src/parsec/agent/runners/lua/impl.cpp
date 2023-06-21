// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "impl.hpp"

#include "crypto/sha256.h"
#include "util/common/keys.hpp"
#include "util/common/variant_overloaded.hpp"

#include <cassert>
#include <secp256k1.h>
#include <secp256k1_schnorrsig.h>

namespace cbdc::parsec::agent::runner {
    static const auto secp_context
        = std::unique_ptr<secp256k1_context,
                          decltype(&secp256k1_context_destroy)>(
            secp256k1_context_create(SECP256K1_CONTEXT_VERIFY),
            &secp256k1_context_destroy);

    lua_runner::lua_runner(std::shared_ptr<logging::log> logger,
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

    auto lua_runner::run() -> bool {
        // TODO: use custom allocator to limit memory allocation
        m_state
            = std::shared_ptr<lua_State>(luaL_newstate(), [](lua_State* s) {
                  lua_close(s);
              });

        if(!m_state) {
            m_log->error("Failed to allocate new lua state");
            m_result_callback(error_code::internal_error);
            return true;
        }

        // TODO: provide custom environment limited only to safe library
        //       methods
        luaL_openlibs(m_state.get());

        lua_register(m_state.get(), "check_sig", &lua_runner::check_sig);

        static constexpr auto function_name = "contract";

        auto load_ret = luaL_loadbufferx(m_state.get(),
                                         m_function.c_str(),
                                         m_function.size(),
                                         function_name,
                                         "b");
        if(load_ret != LUA_OK) {
            m_log->error("Failed to load function chunk");
            m_result_callback(error_code::function_load);
            return true;
        }

        if(lua_pushlstring(m_state.get(), m_param.c_str(), m_param.size())
           == nullptr) {
            m_log->error("Failed to push function params");
            m_result_callback(error_code::internal_error);
            return true;
        }

        schedule_contract();

        return true;
    }

    void lua_runner::contract_epilogue(int n_results) {
        if(n_results != 1) {
            m_log->error("Contract returned more than one result");
            m_result_callback(error_code::result_count);
            return;
        }

        if(lua_istable(m_state.get(), -1) != 1) {
            m_log->error("Contract did not return a table");
            m_result_callback(error_code::result_type);
            return;
        }

        auto results = runtime_locking_shard::state_update_type();

        lua_pushnil(m_state.get());
        while(lua_next(m_state.get(), -2) != 0) {
            auto key_buf = get_stack_string(-2);
            if(!key_buf.has_value()) {
                m_log->error("Result key is not a string");
                m_result_callback(error_code::result_key_type);
                return;
            }
            auto value_buf = get_stack_string(-1);
            if(!value_buf.has_value()) {
                m_log->error("Result value is not a string");
                m_result_callback(error_code::result_value_type);
                return;
            }

            results.emplace(std::move(key_buf.value()),
                            std::move(value_buf.value()));

            lua_pop(m_state.get(), 1);
        }

        m_log->trace(this, "running calling result callback");
        m_result_callback(std::move(results));
        m_log->trace(this, "lua_runner finished contract epilogue");
    }

    auto lua_runner::get_stack_string(int index) -> std::optional<buffer> {
        if(lua_isstring(m_state.get(), index) != 1) {
            return std::nullopt;
        }
        size_t sz{};
        const auto* str = lua_tolstring(m_state.get(), index, &sz);
        assert(str != nullptr);
        auto buf = buffer();
        buf.append(str, sz);
        return buf;
    }

    void lua_runner::schedule_contract() {
        int n_results{};
        auto resume_ret = lua_resume(m_state.get(), nullptr, 1, &n_results);
        if(resume_ret == LUA_YIELD) {
            if(n_results != 1) {
                m_log->error("Contract yielded more than one key");
                m_result_callback(error_code::yield_count);
                return;
            }
            auto key_buf = get_stack_string(-1);
            if(!key_buf.has_value()) {
                m_log->error("Contract did not yield a string");
                m_result_callback(error_code::yield_type);
                return;
            }
            lua_pop(m_state.get(), n_results);
            auto success
                = m_try_lock_callback(std::move(key_buf.value()),
                                      broker::lock_type::write,
                                      [&](auto res) {
                                          handle_try_lock(std::move(res));
                                      });
            if(!success) {
                m_log->error("Failed to issue try lock command");
                m_result_callback(error_code::internal_error);
            }
        } else if(resume_ret != LUA_OK) {
            const auto* err = lua_tostring(m_state.get(), -1);
            m_log->error("Error running contract:", err);
            m_result_callback(error_code::exec_error);
        } else {
            contract_epilogue(n_results);
        }
    }

    void lua_runner::handle_try_lock(
        const broker::interface::try_lock_return_type& res) {
        auto maybe_error = std::visit(
            overloaded{
                [&](const broker::value_type& v) -> std::optional<error_code> {
                    if(lua_pushlstring(m_state.get(), v.c_str(), v.size())
                       == nullptr) {
                        m_log->error("Failed to push yield params");
                        return error_code::internal_error;
                    }
                    return std::nullopt;
                },
                [&](const broker::interface::error_code& /* e */)
                    -> std::optional<error_code> {
                    m_log->error("Broker error acquiring lock");
                    return error_code::lock_error;
                },
                [&](const runtime_locking_shard::shard_error& e)
                    -> std::optional<error_code> {
                    if(e.m_error_code
                       == runtime_locking_shard::error_code::wounded) {
                        return error_code::wounded;
                    }
                    m_log->error("Shard error acquiring lock");
                    return error_code::lock_error;
                }},
            res);
        if(maybe_error.has_value()) {
            m_result_callback(maybe_error.value());
            return;
        }
        schedule_contract();
    }

    auto lua_runner::check_sig(lua_State* L) -> int {
        int n = lua_gettop(L);
        if(n != 3) {
            lua_pushliteral(L, "not enough arguments");
            lua_error(L);
        }
        for(int i = 1; i <= n; i++) {
            if(lua_isstring(L, i) != 1) {
                lua_pushliteral(L, "invalid argument");
                lua_error(L);
            }
        }

        size_t sz{};
        const auto* str = lua_tolstring(L, 1, &sz);
        assert(str != nullptr);
        pubkey_t key{};
        if(sz != key.size()) {
            lua_pushliteral(L, "invalid pubkey");
            lua_error(L);
        }
        std::memcpy(key.data(), str, sz);

        str = lua_tolstring(L, 2, &sz);
        assert(str != nullptr);
        signature_t sig{};
        if(sz != sig.size()) {
            lua_pushliteral(L, "invalid signature");
            lua_error(L);
        }
        std::memcpy(sig.data(), str, sz);

        secp256k1_xonly_pubkey pubkey{};
        if(secp256k1_xonly_pubkey_parse(secp_context.get(),
                                        &pubkey,
                                        key.data())
           != 1) {
            lua_pushliteral(L, "invalid pubkey");
            lua_error(L);
        }

        str = lua_tolstring(L, 3, &sz);
        assert(str != nullptr);
        auto sha = CSHA256();
        auto unsigned_str = std::vector<unsigned char>(sz);
        std::memcpy(unsigned_str.data(), str, sz);
        sha.Write(unsigned_str.data(), sz);
        hash_t sighash{};
        sha.Finalize(sighash.data());

        if(secp256k1_schnorrsig_verify(secp_context.get(),
                                       sig.data(),
                                       sighash.data(),
                                       &pubkey)
           != 1) {
            lua_pushliteral(L, "invalid signature");
            lua_error(L);
        }

        return 0;
    }
}
