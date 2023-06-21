// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "parsec/runtime_locking_shard/impl.hpp"

#include <future>
#include <gtest/gtest.h>

TEST(runtime_locking_shard_test, basic_test) {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);
    auto shard = cbdc::parsec::runtime_locking_shard::impl(log);

    auto ticket_number = 0;
    auto key = cbdc::buffer::from_hex("aa").value();

    auto maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::write,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            auto exp = cbdc::buffer();
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                exp);
        });
    ASSERT_TRUE(maybe_success);

    auto key1 = cbdc::buffer::from_hex("cc").value();

    maybe_success = shard.try_lock(
        ticket_number,
        0,
        key1,
        cbdc::parsec::runtime_locking_shard::lock_type::read,
        false,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            auto exp = cbdc::buffer();
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                cbdc::buffer());
        });
    ASSERT_TRUE(maybe_success);

    auto new_val = cbdc::buffer::from_hex("bb").value();
    maybe_success = shard.prepare(
        ticket_number,
        0,
        {{key, new_val}},
        [](const std::optional<
            cbdc::parsec::runtime_locking_shard::shard_error>& ret) {
            ASSERT_FALSE(ret.has_value());
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.commit(
        ticket_number,
        [](const std::optional<
            cbdc::parsec::runtime_locking_shard::shard_error>& ret) {
            ASSERT_FALSE(ret.has_value());
        });
    ASSERT_TRUE(maybe_success);

    ticket_number++;
    maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::read,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                new_val);
        });
    ASSERT_TRUE(maybe_success);
}

TEST(runtime_locking_shard_test, rollback_test) {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);
    auto shard = cbdc::parsec::runtime_locking_shard::impl(log);

    auto ticket_number = 0;
    auto key = cbdc::buffer::from_hex("aa").value();

    auto maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::write,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            auto exp = cbdc::buffer();
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                exp);
        });
    ASSERT_TRUE(maybe_success);

    auto new_val = cbdc::buffer::from_hex("bb").value();
    maybe_success = shard.prepare(
        ticket_number,
        0,
        {{key, new_val}},
        [](const std::optional<
            cbdc::parsec::runtime_locking_shard::shard_error>& ret) {
            ASSERT_FALSE(ret.has_value());
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.rollback(
        ticket_number,
        [](const std::optional<
            cbdc::parsec::runtime_locking_shard::shard_error>& ret) {
            ASSERT_FALSE(ret.has_value());
        });
    ASSERT_TRUE(maybe_success);

    ticket_number++;
    maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::read,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                cbdc::buffer());
        });
    ASSERT_TRUE(maybe_success);
}

TEST(runtime_locking_shard_test, lock_not_given_test) {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);
    auto shard = cbdc::parsec::runtime_locking_shard::impl(log);

    auto ticket_number = 0;
    auto key = cbdc::buffer::from_hex("aa").value();

    auto maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::write,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            auto exp = cbdc::buffer();
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                exp);
        });
    ASSERT_TRUE(maybe_success);

    auto new_val = cbdc::buffer::from_hex("bb").value();

    maybe_success = shard.try_lock(
        ticket_number + 1,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::read,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                new_val);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.try_lock(
        ticket_number + 2,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::write,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                new_val);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.try_lock(
        ticket_number + 1,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::read,
        false,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(
                std::holds_alternative<
                    cbdc::parsec::runtime_locking_shard::shard_error>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::shard_error>(ret)
                    .m_error_code,
                cbdc::parsec::runtime_locking_shard::error_code::lock_queued);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.prepare(
        ticket_number + 1,
        0,
        {{key, new_val}},
        [](std::optional<cbdc::parsec::runtime_locking_shard::shard_error>
               ret) {
            ASSERT_TRUE(ret.has_value());
            ASSERT_EQ(
                ret.value().m_error_code,
                cbdc::parsec::runtime_locking_shard::error_code::lock_queued);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.prepare(
        ticket_number,
        0,
        {{key, new_val}},
        [](const std::optional<
            cbdc::parsec::runtime_locking_shard::shard_error>& ret) {
            ASSERT_FALSE(ret.has_value());
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.commit(
        ticket_number,
        [](const std::optional<
            cbdc::parsec::runtime_locking_shard::shard_error>& ret) {
            ASSERT_FALSE(ret.has_value());
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.try_lock(
        ticket_number + 1,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::read,
        false,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(
                std::holds_alternative<
                    cbdc::parsec::runtime_locking_shard::shard_error>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::shard_error>(ret)
                    .m_error_code,
                cbdc::parsec::runtime_locking_shard::error_code::lock_held);
        });
    ASSERT_TRUE(maybe_success);

    auto key1 = cbdc::buffer::from_hex("cc").value();

    maybe_success = shard.prepare(
        ticket_number + 1,
        0,
        {{key1, new_val}},
        [](std::optional<cbdc::parsec::runtime_locking_shard::shard_error>
               ret) {
            ASSERT_TRUE(ret.has_value());
            ASSERT_EQ(ret.value().m_error_code,
                      cbdc::parsec::runtime_locking_shard::error_code::
                          lock_not_held);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.prepare(
        ticket_number + 1,
        0,
        {{key, new_val}},
        [](std::optional<cbdc::parsec::runtime_locking_shard::shard_error>
               ret) {
            ASSERT_TRUE(ret.has_value());
            ASSERT_EQ(ret.value().m_error_code,
                      cbdc::parsec::runtime_locking_shard::error_code::
                          state_update_with_read_lock);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.rollback(
        ticket_number + 1,
        [](const std::optional<
            cbdc::parsec::runtime_locking_shard::shard_error>& ret) {
            ASSERT_FALSE(ret.has_value());
        });
    ASSERT_TRUE(maybe_success);
}

TEST(runtime_locking_shard_test, wound_test) {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);
    auto shard = cbdc::parsec::runtime_locking_shard::impl(log);

    auto ticket_number = 1;
    auto key = cbdc::buffer::from_hex("aa").value();

    auto maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::write,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            auto exp = cbdc::buffer();
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                exp);
        });
    ASSERT_TRUE(maybe_success);

    auto new_val = cbdc::buffer::from_hex("bb").value();

    maybe_success = shard.try_lock(
        ticket_number - 1,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::read,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                cbdc::buffer());
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.prepare(
        ticket_number,
        0,
        {{key, new_val}},
        [](std::optional<cbdc::parsec::runtime_locking_shard::shard_error>
               ret) {
            ASSERT_TRUE(ret.has_value());
            ASSERT_EQ(
                ret.value().m_error_code,
                cbdc::parsec::runtime_locking_shard::error_code::wounded);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::read,
        false,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(
                std::holds_alternative<
                    cbdc::parsec::runtime_locking_shard::shard_error>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::shard_error>(ret)
                    .m_error_code,
                cbdc::parsec::runtime_locking_shard::error_code::wounded);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.commit(
        ticket_number,
        [](std::optional<cbdc::parsec::runtime_locking_shard::shard_error>
               ret) {
            ASSERT_TRUE(ret.has_value());
            ASSERT_EQ(
                ret.value().m_error_code,
                cbdc::parsec::runtime_locking_shard::error_code::not_prepared);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.rollback(
        ticket_number,
        [](const std::optional<
            cbdc::parsec::runtime_locking_shard::shard_error>& ret) {
            ASSERT_FALSE(ret.has_value());
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::write,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(
                std::holds_alternative<
                    cbdc::parsec::runtime_locking_shard::shard_error>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::shard_error>(ret)
                    .m_error_code,
                cbdc::parsec::runtime_locking_shard::error_code::wounded);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.rollback(
        ticket_number,
        [](const std::optional<
            cbdc::parsec::runtime_locking_shard::shard_error>& ret) {
            ASSERT_FALSE(ret.has_value());
        });
    ASSERT_TRUE(maybe_success);
}

TEST(runtime_locking_shard_test, prepare_protected_test) {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);
    auto shard = cbdc::parsec::runtime_locking_shard::impl(log);

    auto ticket_number = 1;
    auto key = cbdc::buffer::from_hex("aa").value();

    auto maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::write,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            auto exp = cbdc::buffer();
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                exp);
        });
    ASSERT_TRUE(maybe_success);

    auto new_val = cbdc::buffer::from_hex("bb").value();

    maybe_success = shard.prepare(
        ticket_number,
        0,
        {{key, new_val}},
        [](const std::optional<
            cbdc::parsec::runtime_locking_shard::shard_error>& ret) {
            ASSERT_FALSE(ret.has_value());
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.try_lock(
        ticket_number - 1,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::read,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                new_val);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.commit(
        ticket_number,
        [](const std::optional<
            cbdc::parsec::runtime_locking_shard::shard_error>& ret) {
            ASSERT_FALSE(ret.has_value());
        });
    ASSERT_TRUE(maybe_success);
}

TEST(runtime_locking_shard_test, unknown_ticket_test) {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);
    auto shard = cbdc::parsec::runtime_locking_shard::impl(log);

    auto ticket_number = 0;
    auto key = cbdc::buffer::from_hex("aa").value();

    auto maybe_success = shard.prepare(
        ticket_number,
        0,
        {},
        [](std::optional<cbdc::parsec::runtime_locking_shard::shard_error>
               ret) {
            ASSERT_TRUE(ret.has_value());
            ASSERT_EQ(ret.value().m_error_code,
                      cbdc::parsec::runtime_locking_shard::error_code::
                          unknown_ticket);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.commit(
        ticket_number,
        [](std::optional<cbdc::parsec::runtime_locking_shard::shard_error>
               ret) {
            ASSERT_TRUE(ret.has_value());
            ASSERT_EQ(ret.value().m_error_code,
                      cbdc::parsec::runtime_locking_shard::error_code::
                          unknown_ticket);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.rollback(
        ticket_number,
        [](std::optional<cbdc::parsec::runtime_locking_shard::shard_error>
               ret) {
            ASSERT_TRUE(ret.has_value());
            ASSERT_EQ(ret.value().m_error_code,
                      cbdc::parsec::runtime_locking_shard::error_code::
                          unknown_ticket);
        });
    ASSERT_TRUE(maybe_success);
}

TEST(runtime_locking_shard_test, double_prepare_test) {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);
    auto shard = cbdc::parsec::runtime_locking_shard::impl(log);

    auto ticket_number = 1;
    auto key = cbdc::buffer::from_hex("aa").value();

    auto maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::write,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            auto exp = cbdc::buffer();
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                exp);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.prepare(
        ticket_number,
        0,
        {},
        [](const std::optional<
            cbdc::parsec::runtime_locking_shard::shard_error>& ret) {
            ASSERT_FALSE(ret.has_value());
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::write,
        false,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(
                std::holds_alternative<
                    cbdc::parsec::runtime_locking_shard::shard_error>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::shard_error>(ret)
                    .m_error_code,
                cbdc::parsec::runtime_locking_shard::error_code::prepared);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.prepare(
        ticket_number,
        0,
        {},
        [](std::optional<cbdc::parsec::runtime_locking_shard::shard_error>
               ret) {
            ASSERT_TRUE(ret.has_value());
            ASSERT_EQ(
                ret.value().m_error_code,
                cbdc::parsec::runtime_locking_shard::error_code::prepared);
        });
    ASSERT_TRUE(maybe_success);
}

TEST(runtime_locking_shard_test, upgrade_lock_test) {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);
    auto shard = cbdc::parsec::runtime_locking_shard::impl(log);

    auto ticket_number = 2;
    auto key = cbdc::buffer::from_hex("aa").value();

    auto maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::read,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            auto exp = cbdc::buffer();
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                exp);
        });
    ASSERT_TRUE(maybe_success);

    ticket_number = 1;
    maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::read,
        true,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            auto exp = cbdc::buffer();
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                exp);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::write,
        false,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(std::holds_alternative<
                        cbdc::parsec::runtime_locking_shard::value_type>(ret));
            auto exp = cbdc::buffer();
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::value_type>(ret),
                exp);
        });
    ASSERT_TRUE(maybe_success);

    ticket_number = 2;
    maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::write,
        false,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(
                std::holds_alternative<
                    cbdc::parsec::runtime_locking_shard::shard_error>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::shard_error>(ret)
                    .m_error_code,
                cbdc::parsec::runtime_locking_shard::error_code::wounded);
        });
    ASSERT_TRUE(maybe_success);

    ticket_number = 1;
    maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::write,
        false,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(
                std::holds_alternative<
                    cbdc::parsec::runtime_locking_shard::shard_error>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::shard_error>(ret)
                    .m_error_code,
                cbdc::parsec::runtime_locking_shard::error_code::lock_held);
        });
    ASSERT_TRUE(maybe_success);

    maybe_success = shard.try_lock(
        ticket_number,
        0,
        key,
        cbdc::parsec::runtime_locking_shard::lock_type::read,
        false,
        [&](cbdc::parsec::runtime_locking_shard::interface::
                try_lock_return_type ret) {
            ASSERT_TRUE(
                std::holds_alternative<
                    cbdc::parsec::runtime_locking_shard::shard_error>(ret));
            ASSERT_EQ(
                std::get<cbdc::parsec::runtime_locking_shard::shard_error>(ret)
                    .m_error_code,
                cbdc::parsec::runtime_locking_shard::error_code::lock_held);
        });
    ASSERT_TRUE(maybe_success);
}
