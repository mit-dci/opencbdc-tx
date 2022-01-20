// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "atomizer/messages.hpp"

#include <gtest/gtest.h>

class atomizer_messages_test : public ::testing::Test {
  protected:
    cbdc::buffer m_target_packet{};
    cbdc::buffer_serializer m_ser{m_target_packet};
    cbdc::buffer_serializer m_deser{m_target_packet};
};

TEST_F(atomizer_messages_test, snapshot) {
    auto atm = std::make_shared<cbdc::atomizer::atomizer>(0, 2);
    auto nuraft_snp = nuraft::cs_new<nuraft::snapshot>(
        2,
        5,
        nuraft::cs_new<nuraft::cluster_config>());
    auto blocks
        = std::make_shared<decltype(cbdc::atomizer::state_machine::snapshot::
                                        m_blocks)::element_type>();
    blocks->emplace(5, cbdc::atomizer::block());
    auto snp = cbdc::atomizer::state_machine::snapshot{std::move(atm),
                                                       std::move(nuraft_snp),
                                                       std::move(blocks)};

    ASSERT_TRUE(m_ser << snp);

    auto other_atm = std::make_shared<cbdc::atomizer::atomizer>(3, 2);
    auto other_snp = std::shared_ptr<nuraft::snapshot>();
    auto other_blks
        = std::make_shared<decltype(cbdc::atomizer::state_machine::snapshot::
                                        m_blocks)::element_type>();
    auto deser_snp
        = cbdc::atomizer::state_machine::snapshot{std::move(other_atm),
                                                  std::move(other_snp),
                                                  std::move(other_blks)};
    ASSERT_TRUE(m_deser >> deser_snp);
    ASSERT_EQ(*snp.m_atomizer, *deser_snp.m_atomizer);
    ASSERT_EQ(*snp.m_blocks, *deser_snp.m_blocks);
    ASSERT_EQ(snp.m_snp->get_last_log_term(),
              deser_snp.m_snp->get_last_log_term());
    ASSERT_EQ(snp.m_snp->get_last_log_idx(),
              deser_snp.m_snp->get_last_log_idx());
}
