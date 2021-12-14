// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "atomizer/format.hpp"
#include "coordinator/format.hpp"
#include "sentinel/format.hpp"
#include "serialization/format.hpp"
#include "transaction/messages.hpp"
#include "util.hpp"
#include "watchtower/messages.hpp"
#include "watchtower/status_update_messages.hpp"
#include "watchtower/tx_error_messages.hpp"
#include "watchtower/watchtower.hpp"

#include <gtest/gtest.h>

class PacketIOTest : public ::testing::Test {
  protected:
    cbdc::buffer m_target_packet;
    cbdc::buffer_serializer m_ser{m_target_packet};
    cbdc::buffer_serializer m_deser{m_target_packet};
};

TEST_F(PacketIOTest, outpoint) {
    cbdc::transaction::out_point op;
    op.m_tx_id = {'a', 'b', 'c', 'd'};
    op.m_index = 1;

    m_ser << op;

    cbdc::transaction::out_point result_op;
    m_deser >> result_op;

    ASSERT_EQ(op, result_op);
}

TEST_F(PacketIOTest, output) {
    cbdc::transaction::output out;
    out.m_value = 67;
    out.m_witness_program_commitment = {'t', 'a', 'f', 'm'};

    m_ser << out;

    cbdc::transaction::output result_out;
    m_deser >> result_out;

    ASSERT_EQ(out, result_out);
}

TEST_F(PacketIOTest, input) {
    cbdc::transaction::input in;
    in.m_prevout_data.m_witness_program_commitment = {'a', 'x', 'o', 'p'};
    in.m_prevout_data.m_value = 83;
    in.m_prevout.m_index = 1;
    in.m_prevout.m_tx_id = {'h', 'q', 'l', 'd'};

    m_ser << in;

    cbdc::transaction::input result_in;
    m_deser >> result_in;

    ASSERT_EQ(in, result_in);
}

TEST_F(PacketIOTest, transaction) {
    cbdc::transaction::input in;
    in.m_prevout_data.m_witness_program_commitment = {'a', 'x', 'o', 'p'};
    in.m_prevout_data.m_value = 100;
    in.m_prevout.m_index = 1;
    in.m_prevout.m_tx_id = {'h', 'q', 'l', 'd'};

    cbdc::transaction::output out0;
    out0.m_value = 70;
    out0.m_witness_program_commitment = {'t', 'a', 'f', 'm'};

    cbdc::transaction::output out1;
    out1.m_value = 30;
    out1.m_witness_program_commitment = {'q', 'e', 'n', 'r'};

    cbdc::transaction::full_tx tx;
    tx.m_inputs = {in};
    tx.m_outputs = {out0, out1};
    tx.m_witness.emplace_back(64, std::byte(1));

    m_ser << tx;

    cbdc::transaction::full_tx result_tx;
    m_deser >> result_tx;

    ASSERT_EQ(tx, result_tx);
}

TEST_F(PacketIOTest, ctx_notify_message) {
    cbdc::atomizer::tx_notify_message tx_notify;
    tx_notify.m_attestations.insert({'e', 'o', 'm', 'e'});
    tx_notify.m_block_height = 33;
    tx_notify.m_tx.m_inputs.push_back({'a', 'x', 'o', 'p'});
    tx_notify.m_tx.m_uhs_outputs.push_back({'t', 'a', 'f', 'm'});
    tx_notify.m_tx.m_uhs_outputs.push_back({'q', 'e', 'n', 'r'});
    tx_notify.m_tx.m_id = {'p', 'l', 'k', 'e'};

    m_ser << tx_notify;

    cbdc::atomizer::tx_notify_message result_tx_notify;
    m_deser >> result_tx_notify;

    ASSERT_EQ(tx_notify, result_tx_notify);
}

TEST_F(PacketIOTest, block) {
    cbdc::transaction::compact_tx tx0;
    tx0.m_inputs.push_back({'a', 'x', 'o', 'p'});
    tx0.m_uhs_outputs.push_back({'t', 'a', 'f', 'm'});
    tx0.m_uhs_outputs.push_back({'q', 'e', 'n', 'r'});
    tx0.m_id = {'p', 'l', 'k', 'e'};

    cbdc::transaction::compact_tx tx1;
    tx1.m_inputs.push_back({'h', 'o', 'o', 'e'});
    tx1.m_uhs_outputs.push_back({'q', 'b', 'g', 'y'});
    tx1.m_uhs_outputs.push_back({'m', 'e', 'o', 'b'});
    tx1.m_id = {'o', 'g', 'l', 'j'};

    cbdc::atomizer::block block;
    block.m_height = 1777;
    block.m_transactions = {tx0, tx1};

    m_ser << block;

    cbdc::atomizer::block result_block;
    m_deser >> result_block;

    ASSERT_EQ(block, result_block);
}

TEST_F(PacketIOTest, compact_transaction) {
    cbdc::transaction::compact_tx tx;
    tx.m_inputs.push_back({'h', 'o', 'o', 'e'});
    tx.m_uhs_outputs.push_back({'q', 'b', 'g', 'y'});
    tx.m_uhs_outputs.push_back({'m', 'e', 'o', 'b'});
    tx.m_id = {'o', 'g', 'l', 'j'};

    m_ser << tx;

    cbdc::transaction::compact_tx result_tx;
    m_deser >> result_tx;

    ASSERT_EQ(tx, result_tx);
}

TEST_F(PacketIOTest, tx_error_sync) {
    auto tx_err
        = cbdc::watchtower::tx_error{{'a'}, cbdc::watchtower::tx_error_sync{}};

    m_ser << tx_err;

    auto result_tx_err = cbdc::watchtower::tx_error{m_deser};

    ASSERT_EQ(tx_err, result_tx_err);
}

TEST_F(PacketIOTest, tx_error_stxo_range) {
    auto tx_err
        = cbdc::watchtower::tx_error{{'a'},
                                     cbdc::watchtower::tx_error_stxo_range{}};

    m_ser << tx_err;

    auto result_tx_err = cbdc::watchtower::tx_error{m_deser};

    ASSERT_EQ(tx_err, result_tx_err);
}

TEST_F(PacketIOTest, tx_error_incomplete) {
    auto tx_err
        = cbdc::watchtower::tx_error{{'a'},
                                     cbdc::watchtower::tx_error_incomplete{}};

    m_ser << tx_err;

    auto result_tx_err = cbdc::watchtower::tx_error{m_deser};

    ASSERT_EQ(tx_err, result_tx_err);
}

TEST_F(PacketIOTest, tx_error_input_dne) {
    auto tx_err = cbdc::watchtower::tx_error{
        {'a'},
        cbdc::watchtower::tx_error_inputs_dne{{{'b'}, {'c'}}}};

    m_ser << tx_err;

    auto result_tx_err = cbdc::watchtower::tx_error{m_deser};

    ASSERT_EQ(tx_err, result_tx_err);
}

TEST_F(PacketIOTest, tx_error_inputs_spent) {
    auto tx_err = cbdc::watchtower::tx_error{
        {'a'},
        cbdc::watchtower::tx_error_inputs_spent{{{'b'}, {'c'}}}};

    m_ser << tx_err;

    auto result_tx_err = cbdc::watchtower::tx_error{m_deser};

    ASSERT_EQ(tx_err, result_tx_err);
}

TEST_F(PacketIOTest, status_update_request_check) {
    auto su_req = cbdc::watchtower::status_update_request{
        {{{'t', 'x', 'a'}, {{'u', 'a'}, {'u', 'b'}}},
         {{'t', 'x', 'b'}, {{'u', 'c'}, {'u', 'd'}}}}};

    m_ser << su_req;

    auto result_su_req = cbdc::watchtower::status_update_request(m_deser);

    ASSERT_EQ(su_req, result_su_req);
}

TEST_F(PacketIOTest, status_update_response_check_success) {
    auto su_req = cbdc::watchtower::status_request_check_success{
        {{{'t', 'x', 'a'},
          {cbdc::watchtower::status_update_state{
               cbdc::watchtower::search_status::no_history,
               0,
               {'u', 'a'}},
           cbdc::watchtower::status_update_state{
               cbdc::watchtower::search_status::spent,
               5,
               {'u', 'b'}}}},
         {{'t', 'x', 'b'},
          {cbdc::watchtower::status_update_state{
              cbdc::watchtower::search_status::unspent,
              2,
              {'u', 'c'}}}}}};

    m_ser << su_req;

    auto result_su_req
        = cbdc::watchtower::status_request_check_success(m_deser);

    ASSERT_EQ(su_req, result_su_req);
}

TEST_F(PacketIOTest, best_block_height_request) {
    auto bbh_req = cbdc::watchtower::best_block_height_request{};

    m_ser << bbh_req;

    auto result_bbh_req = cbdc::watchtower::best_block_height_request(m_deser);

    ASSERT_EQ(bbh_req, result_bbh_req);
}

TEST_F(PacketIOTest, best_block_height_response) {
    auto bbh_res = cbdc::watchtower::best_block_height_response{667};

    m_ser << bbh_res;

    auto result_bbh_res
        = cbdc::watchtower::best_block_height_response(m_deser);

    ASSERT_EQ(bbh_res, result_bbh_res);
}

TEST_F(PacketIOTest, watchtower_request_su) {
    auto req
        = cbdc::watchtower::request{cbdc::watchtower::status_update_request{
            {{{'t', 'x', 'a'}, {{'u', 'a'}, {'u', 'b'}}},
             {{'t', 'x', 'b'}, {{'u', 'c'}, {'u', 'd'}}}}}};

    m_ser << req;

    auto result_req = cbdc::watchtower::request(m_deser);

    ASSERT_EQ(req, result_req);
}

TEST_F(PacketIOTest, watchtower_request_bbh) {
    auto req = cbdc::watchtower::request{
        cbdc::watchtower::best_block_height_request{}};

    m_ser << req;

    auto result_req = cbdc::watchtower::request(m_deser);

    ASSERT_EQ(req, result_req);
}

TEST_F(PacketIOTest, watchtower_response_su) {
    auto resp = cbdc::watchtower::response{
        cbdc::watchtower::status_request_check_success{{
            {{'t', 'x', 'a'},
             {cbdc::watchtower::status_update_state{
                 cbdc::watchtower::search_status::no_history,
                 0,
                 {'u', 'a'}}}},
        }}};

    m_ser << resp;

    auto resp_deser = cbdc::watchtower::response(m_deser);

    ASSERT_EQ(resp, resp_deser);
}

TEST_F(PacketIOTest, watchtower_response_bbh) {
    auto resp = cbdc::watchtower::response{
        cbdc::watchtower::best_block_height_response{33}};

    m_ser << resp;

    auto resp_deser = cbdc::watchtower::response(m_deser);

    ASSERT_EQ(resp, resp_deser);
}

TEST_F(PacketIOTest, TXErrorCodeTest) {
    auto tx_err = cbdc::transaction::validation::tx_error(
        cbdc::transaction::validation::tx_error_code::asymmetric_values);
    m_ser << tx_err;
    auto tx_err_deser = cbdc::transaction::validation::tx_error(
        cbdc::transaction::validation::tx_error_code::no_outputs);
    m_deser >> tx_err_deser;
    ASSERT_EQ(tx_err, tx_err_deser);
}

TEST_F(PacketIOTest, InputErrorTest) {
    auto tx_err = cbdc::transaction::validation::tx_error{
        cbdc::transaction::validation::input_error{
            cbdc::transaction::validation::input_error_code::duplicate,
            std::nullopt,
            5}};
    m_ser << tx_err;
    auto tx_err_deser = cbdc::transaction::validation::tx_error{
        cbdc::transaction::validation::tx_error_code::no_outputs};
    m_deser >> tx_err_deser;
    ASSERT_EQ(tx_err, tx_err_deser);

    tx_err = cbdc::transaction::validation::tx_error{
        cbdc::transaction::validation::input_error{
            cbdc::transaction::validation::input_error_code::data_error,
            cbdc::transaction::validation::output_error_code::zero_value,
            5}};
    m_ser.reset();
    m_ser << tx_err;
    m_deser.reset();
    m_deser >> tx_err_deser;
    ASSERT_EQ(tx_err, tx_err_deser);
}

TEST_F(PacketIOTest, OutputErrorTest) {
    auto tx_err = cbdc::transaction::validation::tx_error{
        cbdc::transaction::validation::output_error{
            cbdc::transaction::validation::output_error_code::zero_value,
            20}};
    m_ser << tx_err;
    auto tx_err_deser = cbdc::transaction::validation::tx_error{
        cbdc::transaction::validation::tx_error_code::no_outputs};
    m_deser >> tx_err_deser;
    ASSERT_EQ(tx_err, tx_err_deser);
}

TEST_F(PacketIOTest, WitnessErrorTest) {
    auto tx_err = cbdc::transaction::validation::tx_error{
        cbdc::transaction::validation::witness_error{
            cbdc::transaction::validation::witness_error_code::malformed,
            10}};
    m_ser << tx_err;
    auto tx_err_deser = cbdc::transaction::validation::tx_error(
        cbdc::transaction::validation::tx_error_code::no_outputs);
    m_deser >> tx_err_deser;
    ASSERT_EQ(tx_err, tx_err_deser);
}

TEST_F(PacketIOTest, SentinelResponseInvalidTest) {
    auto tx_err = cbdc::transaction::validation::tx_error{
        cbdc::transaction::validation::output_error{
            cbdc::transaction::validation::output_error_code::zero_value,
            20}};
    auto resp
        = cbdc::sentinel::response{cbdc::sentinel::tx_status::static_invalid,
                                   tx_err};
    m_ser << resp;
    auto resp_deser = cbdc::sentinel::response{};
    m_deser >> resp_deser;
    ASSERT_EQ(resp, resp_deser);
}

TEST_F(PacketIOTest, SentinelResponsePendingTest) {
    auto resp = cbdc::sentinel::response{cbdc::sentinel::tx_status::pending,
                                         std::nullopt};
    m_ser << resp;
    auto resp_deser = cbdc::sentinel::response{};
    m_deser >> resp_deser;
    ASSERT_EQ(resp, resp_deser);
}

TEST_F(PacketIOTest, empty_optional_test) {
    auto opt = std::optional<cbdc::atomizer::block>();
    m_ser << opt;
    auto resp_opt = std::optional<cbdc::atomizer::block>();
    m_deser >> resp_opt;
    ASSERT_EQ(opt, resp_opt);
}

TEST_F(PacketIOTest, optional_test) {
    auto opt = std::optional<cbdc::atomizer::block>();
    auto blk = cbdc::atomizer::block();
    blk.m_height = 50;
    auto tx = cbdc::transaction::full_tx();
    blk.m_transactions.emplace_back(tx);
    opt = blk;
    m_ser << opt;
    auto resp_opt = std::optional<cbdc::atomizer::block>();
    m_deser >> resp_opt;
    ASSERT_EQ(opt, resp_opt);
}

TEST_F(PacketIOTest, aggregate_tx_notify) {
    auto atn = cbdc::atomizer::aggregate_tx_notify();
    atn.m_oldest_attestation = 77;
    atn.m_tx = cbdc::test::simple_tx({'t', 'x', 'a'},
                                     {{'a'}, {'b'}},
                                     {{'c'}, {'d'}});
    m_ser << atn;

    auto atn_deser = cbdc::atomizer::aggregate_tx_notify();
    m_deser >> atn_deser;
    ASSERT_EQ(atn, atn_deser);
}

TEST_F(PacketIOTest, aggregate_tx_notify_set) {
    auto atns = cbdc::atomizer::aggregate_tx_notify_set();
    atns.m_cmd = cbdc::atomizer::state_machine::command::tx_notify;

    auto atn = cbdc::atomizer::aggregate_tx_notify();
    atn.m_oldest_attestation = 77;
    atn.m_tx = cbdc::test::simple_tx({'t', 'x', 'a'},
                                     {{'a'}, {'b'}},
                                     {{'c'}, {'d'}});

    atns.m_agg_txs.push_back(atn);

    m_ser << atns;

    auto atns_deser = cbdc::atomizer::aggregate_tx_notify_set();
    m_deser >> atns_deser;
    ASSERT_EQ(atns, atns_deser);
}

TEST_F(PacketIOTest, variant) {
    auto outpoint = cbdc::transaction::out_point{{'a', 'b', 'c', 'd'}, 1};
    auto output = cbdc::transaction::output{{'b', 'c'}, 100};
    auto var = std::variant<cbdc::transaction::out_point,
                            cbdc::transaction::output>(outpoint);
    m_ser << var;
    var = output;

    m_deser >> var;
    ASSERT_TRUE(std::holds_alternative<cbdc::transaction::out_point>(var));
    ASSERT_EQ(outpoint, std::get<cbdc::transaction::out_point>(var));

    m_target_packet.clear();
    m_ser.reset();
    m_deser.reset();

    var = output;
    m_ser << var;
    var = outpoint;
    m_deser >> var;
    ASSERT_TRUE(std::holds_alternative<cbdc::transaction::output>(var));
    ASSERT_EQ(output, std::get<cbdc::transaction::output>(var));
}
