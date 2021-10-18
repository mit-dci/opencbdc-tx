// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/atomizer/watchtower/watchtower.hpp"
#include "util.hpp"

#include <gtest/gtest.h>

class WatchtowerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        cbdc::atomizer::block b0;
        b0.m_height = m_best_height;
        b0.m_transactions.push_back(
            cbdc::test::simple_tx({'A'}, {{{'b'}}, {{'C'}}}, {{{'d'}}}));
        b0.m_transactions.push_back(
            cbdc::test::simple_tx({'E'}, {{{'d'}}, {{'f'}}}, {{{'G'}}}));
        b0.m_transactions.push_back(
            cbdc::test::simple_tx({'h'}, {{{'i'}}, {{'j'}}}, {{{'k'}}}));
        m_watchtower.add_block(std::move(b0));
    }

    cbdc::watchtower::watchtower m_watchtower{0, 0};
    static constexpr auto m_best_height{44};
};

TEST_F(WatchtowerTest, check_spent) {
    auto res = m_watchtower.handle_status_update_request(
        cbdc::watchtower::status_update_request{{{{'A'}, {{'C'}}}}});

    ASSERT_EQ(*res,
              (cbdc::watchtower::response{
                  cbdc::watchtower::status_request_check_success{
                      {{{'A'},
                        {{cbdc::watchtower::status_update_state{
                            cbdc::watchtower::search_status::spent,
                            44,
                            {'C'}}}}}}}}));
}

TEST_F(WatchtowerTest, check_unspent) {
    auto res = m_watchtower.handle_status_update_request(
        cbdc::watchtower::status_update_request{{{{'E'}, {{'G'}}}}});

    ASSERT_EQ(*res,
              (cbdc::watchtower::response{
                  cbdc::watchtower::status_request_check_success{
                      {{{'E'},
                        {cbdc::watchtower::status_update_state{
                            cbdc::watchtower::search_status::unspent,
                            44,
                            {'G'}}}}}}}));
}

TEST_F(WatchtowerTest, internal_error_tx) {
    std::vector<cbdc::watchtower::tx_error> errs{
        cbdc::watchtower::tx_error{{'t', 'x', 'a'},
                                   cbdc::watchtower::tx_error_stxo_range{}},
        cbdc::watchtower::tx_error{{'t', 'x', 'b'},
                                   cbdc::watchtower::tx_error_sync{}}};
    m_watchtower.add_errors(std::move(errs));

    auto res = m_watchtower.handle_status_update_request(
        cbdc::watchtower::status_update_request{
            {{{'t', 'x', 'a'}, {{'a'}, {'b'}}},
             {{'t', 'x', 'b'}, {{'c'}, {'d'}}}}});

    ASSERT_EQ(*res,
              (cbdc::watchtower::response{
                  cbdc::watchtower::status_request_check_success{
                      {{{'t', 'x', 'a'},
                        {cbdc::watchtower::status_update_state{
                             cbdc::watchtower::search_status::internal_error,
                             m_best_height,
                             {'a'}},
                         cbdc::watchtower::status_update_state{
                             cbdc::watchtower::search_status::internal_error,
                             m_best_height,
                             {'b'}}}},
                       {{'t', 'x', 'b'},
                        {cbdc::watchtower::status_update_state{
                             cbdc::watchtower::search_status::internal_error,
                             m_best_height,
                             {'c'}},
                         cbdc::watchtower::status_update_state{
                             cbdc::watchtower::search_status::internal_error,
                             m_best_height,
                             {'d'}}}}}}}));
}

TEST_F(WatchtowerTest, invalid_input_error) {
    std::vector<cbdc::watchtower::tx_error> errs{
        cbdc::watchtower::tx_error{
            {'t', 'x', 'a'},
            cbdc::watchtower::tx_error_inputs_dne{{{'a'}}}},
        // Double spend of an existing input with a new transaction.
        cbdc::watchtower::tx_error{
            {'t', 'x', 'b'},
            cbdc::watchtower::tx_error_inputs_spent{{{'C'}}}},
        // Double spend of an existing input through a previously transmitted
        // and accepted transaction, emitted from a shard after the first
        // transaction has cleared the atomizer.
        cbdc::watchtower::tx_error{
            {'A'},
            cbdc::watchtower::tx_error_inputs_dne{{{'C'}}}},
        // Double spend of an existing input through a previously transmitted
        // and accepted transaction, emitted during atomizer processing.
        cbdc::watchtower::tx_error{
            {'A'},
            cbdc::watchtower::tx_error_inputs_spent{{{'C'}}}}};
    m_watchtower.add_errors(std::move(errs));

    auto res = m_watchtower.handle_status_update_request(
        cbdc::watchtower::status_update_request{
            {{{'t', 'x', 'a'}, {{'a'}, {'b'}}},
             {{'t', 'x', 'b'}, {{'C'}, {'d'}}},
             {{'A'}, {{'C'}}}}});

    ASSERT_EQ(*res,
              (cbdc::watchtower::response{
                  cbdc::watchtower::status_request_check_success{
                      {{{'t', 'x', 'a'},
                        {cbdc::watchtower::status_update_state{
                             cbdc::watchtower::search_status::invalid_input,
                             m_best_height,
                             {'a'}},
                         cbdc::watchtower::status_update_state{
                             cbdc::watchtower::search_status::tx_rejected,
                             m_best_height,
                             {'b'}}}},
                       {{'t', 'x', 'b'},
                        {cbdc::watchtower::status_update_state{
                             cbdc::watchtower::search_status::invalid_input,
                             m_best_height,
                             {'C'}},
                         cbdc::watchtower::status_update_state{
                             cbdc::watchtower::search_status::tx_rejected,
                             m_best_height,
                             {'d'}}}},
                       {{'A'},
                        {cbdc::watchtower::status_update_state{
                            cbdc::watchtower::search_status::spent,
                            m_best_height,
                            {'C'}}}}}}}));
}

TEST_F(WatchtowerTest, best_block_height) {
    auto res = m_watchtower.handle_best_block_height_request(
        cbdc::watchtower::best_block_height_request{});

    ASSERT_EQ(*res,
              (cbdc::watchtower::response{
                  cbdc::watchtower::best_block_height_response{44}}));
}
