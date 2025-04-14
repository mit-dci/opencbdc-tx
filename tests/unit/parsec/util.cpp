// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.hpp"

#include <gtest/gtest.h>

namespace cbdc::test {
    void add_to_shard(std::shared_ptr<cbdc::parsec::broker::interface> broker,
                      cbdc::buffer key,
                      cbdc::buffer value) {
        auto begin_res = broker->get_new_ticket_number([&](auto begin_ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::ticket_machine::ticket_number_type>(
                begin_ret));
            auto ticket_number
                = std::get<cbdc::parsec::ticket_machine::ticket_number_type>(
                    begin_ret);
            auto lock_res = broker->try_lock(
                ticket_number,
                key,
                cbdc::parsec::runtime_locking_shard::lock_type::write,
                [&](auto try_lock_res) {
                    ASSERT_TRUE(
                        std::holds_alternative<cbdc::buffer>(try_lock_res));
                    auto commit_res = broker->commit(
                        ticket_number,
                        {{key, value}},
                        [&](auto commit_ret) {
                            ASSERT_FALSE(commit_ret.has_value());
                            auto finish_res = broker->finish(
                                ticket_number,
                                [&](auto finish_ret) {
                                    ASSERT_FALSE(finish_ret.has_value());
                                });
                            ASSERT_TRUE(finish_res);
                        });
                    ASSERT_TRUE(commit_res);
                });
            ASSERT_TRUE(lock_res);
        });
        ASSERT_TRUE(begin_res);
    }
}
