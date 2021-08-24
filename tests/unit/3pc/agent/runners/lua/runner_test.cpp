// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "3pc/agent/runners/lua/impl.hpp"
#include "3pc/util.hpp"

#include <gtest/gtest.h>

TEST(agent_runner_test, rollback_test) {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);

    auto cfg = cbdc::threepc::config();

    static constexpr auto contract
        = "1b4c7561540019930d0a1a0a0408087856000000000000000000000028774001808"
          "1860100038d8b0000018e00010203810100c40002020f0000019300000052000000"
          "0f0004018b000004928003058b000004c8000200c700010086048276048a636f726"
          "f7574696e6504867969656c64048668656c6c6f0482740483686981000000808080"
          "8080";
    auto func = cbdc::buffer::from_hex(contract).value();
    auto param = cbdc::buffer();

    static constexpr auto exp_val = "hi";
    auto exp_val_buf = cbdc::buffer();
    exp_val_buf.append(exp_val, 2);

    static constexpr auto exp_key = "hello";
    auto exp_key_buf = cbdc::buffer();
    exp_key_buf.append(exp_key, 5);

    auto result_cb =
        [&](cbdc::threepc::agent::runner::interface::run_return_type ret) {
            ASSERT_TRUE(
                std::holds_alternative<
                    cbdc::threepc::runtime_locking_shard::state_update_type>(
                    ret));
            auto& val = std::get<
                cbdc::threepc::runtime_locking_shard::state_update_type>(ret);
            ASSERT_EQ(val.size(), 1UL);
            ASSERT_EQ(val[exp_key_buf], exp_val_buf);
        };

    auto try_lock_cb
        = [&](const cbdc::threepc::broker::key_type& key,
              cbdc::threepc::broker::lock_type /* locktype */,
              const cbdc::threepc::broker::interface::try_lock_callback_type&
                  res_cb) -> bool {
        EXPECT_EQ(key, exp_key_buf);
        res_cb(cbdc::buffer());
        return true;
    };

    auto runner
        = cbdc::threepc::agent::runner::lua_runner(log,
                                                   cfg,
                                                   std::move(func),
                                                   std::move(param),
                                                   false,
                                                   std::move(result_cb),
                                                   std::move(try_lock_cb),
                                                   nullptr,
                                                   nullptr,
                                                   nullptr,
                                                   0);
    ASSERT_TRUE(runner.run());
}
