// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transaction/transaction.hpp"

#include <gtest/gtest.h>

TEST(CTransaction, input_from_output_basic) {
    cbdc::transaction::full_tx tx;
    cbdc::transaction::output send_out;
    cbdc::transaction::output recieve_out;

    send_out.m_value = 40;
    send_out.m_witness_program_commitment = {'a', 'b', 'c', 'd'};
    tx.m_outputs.push_back(send_out);

    recieve_out.m_value = 60;
    recieve_out.m_witness_program_commitment = {'e', 'f', 'g', 'h'};
    tx.m_outputs.push_back(recieve_out);

    auto send_result = cbdc::transaction::input_from_output(tx, 0);
    ASSERT_TRUE(send_result);
    ASSERT_EQ(send_result->m_prevout.m_tx_id, cbdc::transaction::tx_id(tx));
    ASSERT_EQ(send_result->m_prevout.m_index, uint32_t{0});
    ASSERT_EQ(send_result->m_prevout_data.m_value, uint32_t{40});

    auto recieve_result = cbdc::transaction::input_from_output(tx, 1);
    ASSERT_TRUE(recieve_result);
    ASSERT_EQ(recieve_result->m_prevout.m_tx_id, cbdc::transaction::tx_id(tx));
    ASSERT_EQ(recieve_result->m_prevout.m_index, uint32_t{1});
    ASSERT_EQ(recieve_result->m_prevout_data.m_value, uint32_t{60});
}

TEST(CTransaction, input_from_output_out_of_bounds) {
    cbdc::transaction::full_tx tx;

    auto result = cbdc::transaction::input_from_output(tx, 1);
    ASSERT_FALSE(result);
}
