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
#include "parsec/util.hpp"
#include "util/common/keys.hpp"
#include "util/serialization/buffer_serializer.hpp"
#include "util/serialization/format.hpp"

#include <future>
#include <gtest/gtest.h>
#include <lua.hpp>
#include <secp256k1.h>
#include <secp256k1_schnorrsig.h>
#include <thread>

TEST(lua_runner_test, lua_write_lock_test) {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dofile(L,
                "../tests/unit/parsec/agent/runners/lua/test_write_locks.lua");
    lua_getglobal(L, "gen_bytecode");
    ASSERT_EQ(lua_pcall(L, 0, 1, 0), 0);
    auto contract = cbdc::buffer::from_hex(lua_tostring(L, -1)).value();
    auto func = cbdc::buffer::from_hex(contract.to_hex()).value();
    auto param = cbdc::buffer();
    auto cfg = cbdc::parsec::config();

    auto result_cb = [&](cbdc::parsec::agent::runner::interface::
                             run_return_type /* value */) {
        return;
    };

    std::promise<void> write_lock_promise;
    auto write_lock_future = write_lock_promise.get_future();
    auto try_lock_cb
        = [&](const cbdc::parsec::broker::key_type& key,
              cbdc::parsec::broker::lock_type locktype,
              const cbdc::parsec::broker::interface::try_lock_callback_type&
              /* res_cb */) -> bool {
        // Cannot use ASSERT here because it does not satisfy return
        // requirements so we use expect
        if(std::string("W") == key.c_str()) {
            EXPECT_TRUE(locktype == cbdc::parsec::broker::lock_type::write);
            write_lock_promise.set_value();
        } else {
            // Cannot use FAIL() here because it expands to a return statement
            // which does not satisfy the requirements of a try_lock_callback
            // so we improvise
            EXPECT_FALSE(true);
        }
        return true;
    };

    auto runner
        = cbdc::parsec::agent::runner::lua_runner(log,
                                                  cfg,
                                                  std::move(func),
                                                  std::move(param),
                                                  false,
                                                  std::move(result_cb),
                                                  std::move(try_lock_cb),
                                                  nullptr,
                                                  nullptr,
                                                  0);
    auto res = runner.run();
    write_lock_future.get();
    ASSERT_TRUE(res);
}

TEST(lua_runner_test, lua_read_lock_test) {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dofile(L,
                "../tests/unit/parsec/agent/runners/lua/test_read_locks.lua");
    lua_getglobal(L, "gen_bytecode");
    ASSERT_EQ(lua_pcall(L, 0, 1, 0), 0);
    auto contract = cbdc::buffer::from_hex(lua_tostring(L, -1)).value();
    auto func = cbdc::buffer::from_hex(contract.to_hex()).value();
    auto param = cbdc::buffer();
    auto cfg = cbdc::parsec::config();

    auto result_cb = [&](cbdc::parsec::agent::runner::interface::
                             run_return_type /* value */) {
        return;
    };

    std::promise<void> read_lock_promise;
    auto read_lock_future = read_lock_promise.get_future();
    auto try_lock_cb
        = [&](const cbdc::parsec::broker::key_type& key,
              cbdc::parsec::broker::lock_type locktype,
              const cbdc::parsec::broker::interface::try_lock_callback_type&
              /* res_cb */) -> bool {
        // Cannot use ASSERT here because it does not satisfy return
        // requirements so we use expect
        if(std::string("R") == key.c_str()) {
            EXPECT_TRUE(locktype == cbdc::parsec::broker::lock_type::read);
            read_lock_promise.set_value();
        } else {
            // Cannot use FAIL() here because it expands to a return statement
            // which does not satisfy the requirements of a try_lock_callback
            // so we improvise
            EXPECT_FALSE(true);
        }
        return true;
    };

    auto runner
        = cbdc::parsec::agent::runner::lua_runner(log,
                                                  cfg,
                                                  std::move(func),
                                                  std::move(param),
                                                  false,
                                                  std::move(result_cb),
                                                  std::move(try_lock_cb),
                                                  nullptr,
                                                  nullptr,
                                                  0);
    auto res = runner.run();
    read_lock_future.get();
    ASSERT_TRUE(res);
}
