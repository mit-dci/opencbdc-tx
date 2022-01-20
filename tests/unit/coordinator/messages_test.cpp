// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coordinator/format.hpp"
#include "raft/serialization.hpp"
#include "serialization/util.hpp"
#include "transaction/messages.hpp"
#include "util.hpp"

#include <gtest/gtest.h>

class coordinator_messages_test : public ::testing::Test {
  protected:
    cbdc::buffer m_target_packet{};
    cbdc::buffer_serializer m_ser{m_target_packet};
    cbdc::buffer_serializer m_deser{m_target_packet};

    cbdc::test::compact_transaction m_tx{
        cbdc::test::simple_tx({'a', 'b', 'c'},
                              {{'d', 'e', 'f'}, {'g', 'h', 'i'}},
                              {{'x', 'y', 'z'}, {'z', 'z', 'z'}})};
};

TEST_F(coordinator_messages_test, command_header) {
    auto comm = cbdc::coordinator::controller::sm_command_header{
        cbdc::coordinator::state_machine::command::commit,
        cbdc::hash_t{'a'}};

    ASSERT_TRUE(m_ser << comm);

    auto deser_comm = cbdc::coordinator::controller::sm_command_header();
    ASSERT_TRUE(m_deser >> deser_comm);
    ASSERT_EQ(comm, deser_comm);
}

TEST_F(coordinator_messages_test, prepare_command) {
    auto header = cbdc::coordinator::controller::sm_command_header{
        cbdc::coordinator::state_machine::command::prepare,
        cbdc::hash_t{'a'}};
    auto param = cbdc::coordinator::controller::prepare_tx{m_tx};
    auto comm = cbdc::coordinator::controller::sm_command{header, param};

    ASSERT_TRUE(m_ser << comm);

    auto deser_header = cbdc::coordinator::controller::sm_command_header();
    ASSERT_TRUE(m_deser >> deser_header);
    ASSERT_EQ(header, deser_header);

    auto deser_comm = cbdc::coordinator::controller::prepare_tx();
    ASSERT_TRUE(m_deser >> deser_comm);
    ASSERT_EQ(param, deser_comm);
}

TEST_F(coordinator_messages_test, commit_command) {
    auto header = cbdc::coordinator::controller::sm_command_header{
        cbdc::coordinator::state_machine::command::commit,
        cbdc::hash_t{'a'}};
    auto param
        = cbdc::coordinator::controller::commit_tx{{true, false}, {{0}, {5}}};
    auto comm = cbdc::coordinator::controller::sm_command{header, param};

    ASSERT_TRUE(m_ser << comm);

    auto deser_header = cbdc::coordinator::controller::sm_command_header();
    ASSERT_TRUE(m_deser >> deser_header);
    ASSERT_EQ(header, deser_header);

    auto deser_comm = cbdc::coordinator::controller::commit_tx();
    ASSERT_TRUE(m_deser >> deser_comm);
    ASSERT_EQ(param, deser_comm);
}

TEST_F(coordinator_messages_test, get_command) {
    auto header = cbdc::coordinator::controller::sm_command_header{
        cbdc::coordinator::state_machine::command::get};
    auto comm = cbdc::coordinator::controller::sm_command{header};

    ASSERT_TRUE(m_ser << comm);

    auto deser_header = cbdc::coordinator::controller::sm_command_header();
    ASSERT_TRUE(m_deser >> deser_header);
    ASSERT_EQ(header, deser_header);
    ASSERT_TRUE(m_deser.end_of_buffer());
}

TEST_F(coordinator_messages_test, coordinator_state) {
    auto prep_param = cbdc::coordinator::controller::prepare_tx{m_tx, m_tx};
    auto prep = cbdc::coordinator::controller::prepare_txs{
        {cbdc::hash_t{'b'}, prep_param}};
    auto comm_param
        = cbdc::coordinator::controller::commit_tx{{true, false}, {{0}, {5}}};
    auto comm = cbdc::coordinator::controller::commit_txs{
        {cbdc::hash_t{'c'}, comm_param}};
    auto disc = cbdc::coordinator::controller::discard_txs{cbdc::hash_t{'d'}};
    auto state
        = cbdc::coordinator::controller::coordinator_state{prep, comm, disc};

    auto sm_state = cbdc::coordinator::state_machine::coordinator_state();
    auto prep_buf = nuraft::buffer::alloc(cbdc::serialized_size(prep_param));
    auto prep_ser = cbdc::nuraft_serializer(*prep_buf);
    ASSERT_TRUE(prep_ser << prep_param);
    sm_state.m_prepare_txs.emplace(cbdc::hash_t{'b'}, prep_buf);

    auto comm_buf = nuraft::buffer::alloc(cbdc::serialized_size(comm_param));
    auto comm_ser = cbdc::nuraft_serializer(*comm_buf);
    ASSERT_TRUE(comm_ser << comm_param);
    sm_state.m_commit_txs.emplace(cbdc::hash_t{'c'}, comm_buf);
    sm_state.m_discard_txs.emplace(cbdc::hash_t{'d'});

    ASSERT_TRUE(m_ser << sm_state);

    auto deser_state = cbdc::coordinator::controller::coordinator_state();
    ASSERT_TRUE(m_deser >> deser_state);
    ASSERT_EQ(state, deser_state);
}
