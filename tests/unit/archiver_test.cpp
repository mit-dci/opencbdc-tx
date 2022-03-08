// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uhs/atomizer/archiver/controller.hpp"
#include "uhs/atomizer/atomizer/format.hpp"
#include "util/common/logging.hpp"
#include "util/serialization/format.hpp"

#include <filesystem>
#include <gtest/gtest.h>

class ArchiverTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_log = std::make_shared<cbdc::logging::log>(
            cbdc::logging::log_level::fatal);

        cbdc::network::endpoint_t atomizer_endpoint
            = {"non-existent-host", 5001};
        m_config_opts.m_atomizer_endpoints.emplace_back(atomizer_endpoint);
        cbdc::network::endpoint_t archiver_endpoint = {"127.0.0.1", 5000};
        m_config_opts.m_archiver_endpoints.emplace_back(archiver_endpoint);
        m_config_opts.m_archiver_db_dirs.emplace_back("archiver0_db");
        m_archiver
            = std::make_unique<cbdc::archiver::controller>(0,
                                                           m_config_opts,
                                                           m_log,
                                                           0);

        make_dummy_blocks();
    }
    void TearDown() override {
        std::filesystem::remove_all("archiver0_db");
        std::filesystem::remove_all("tp_samples.txt");
    }

    void make_dummy_blocks() {
        uint8_t val{0};
        static constexpr auto n_blocks = 10;
        static constexpr auto n_txs = 20;
        for(int h{0}; h < n_blocks; h++) {
            cbdc::atomizer::block b;
            b.m_height = h + 1;
            for(int i{0}; i < n_txs; i++) {
                cbdc::transaction::full_tx tx{};

                cbdc::transaction::input inp{};
                inp.m_prevout.m_tx_id = {val++};
                inp.m_prevout.m_index = val++;
                inp.m_prevout_data.m_witness_program_commitment = {val++};
                inp.m_prevout_data.m_value = val++;

                cbdc::transaction::output out;
                out.m_witness_program_commitment = {val++};
                out.m_value = val++;

                tx.m_inputs.push_back(inp);
                tx.m_outputs.push_back(out);

                const auto compact_tx = cbdc::transaction::compact_tx(tx);

                b.m_transactions.push_back(compact_tx);
            }
            m_dummy_blocks.push_back(b);
        }
    }

    std::vector<cbdc::atomizer::block> m_dummy_blocks;

    cbdc::config::options m_config_opts{};
    std::shared_ptr<cbdc::logging::log> m_log{};
    std::unique_ptr<cbdc::archiver::controller> m_archiver{};
};

TEST_F(ArchiverTest, archiver_running) {
    ASSERT_TRUE(m_archiver->running());
}

TEST_F(ArchiverTest, archiver_leveldb_init) {
    ASSERT_TRUE(m_archiver->init_leveldb());
}

TEST_F(ArchiverTest, archiver_leveldb_init_failure) {
    std::filesystem::remove_all("archiver0_db");
    // Make a file with the same name as the DB directory
    // That will make the init fail

    std::ofstream fs("archiver0_db", std::ios::out);
    fs.close();
    ASSERT_FALSE(m_archiver->init_leveldb());
}

// Test if the best block height is properly initialized to zero
TEST_F(ArchiverTest, archiver_best_block_init) {
    ASSERT_TRUE(m_archiver->init_leveldb());
    ASSERT_TRUE(m_archiver->init_best_block());
    ASSERT_EQ(m_archiver->best_block_height(), 0);
}

// Test if the best block height is properly initialized to non-zero
TEST_F(ArchiverTest, archiver_best_block_init_nonzero) {
    {
        auto archiver0
            = std::make_unique<cbdc::archiver::controller>(0,
                                                           m_config_opts,
                                                           m_log,
                                                           5);

        archiver0->init_leveldb();
        archiver0->digest_block(m_dummy_blocks[0]);
        ASSERT_EQ(archiver0->best_block_height(), 1);
    }
    {
        auto archiver1
            = std::make_unique<cbdc::archiver::controller>(0,
                                                           m_config_opts,
                                                           m_log,
                                                           5);

        archiver1->init_leveldb();
        archiver1->init_best_block();
        ASSERT_EQ(archiver1->best_block_height(), 1);
    }
}

// Test if sample collection succeeds to initialize
TEST_F(ArchiverTest, archiver_sample_collection_init) {
    ASSERT_TRUE(m_archiver->init_sample_collection());
}

// Test if sample collection fails properly when the tp_samples.txt file
// cannot be created
TEST_F(ArchiverTest, archiver_sample_collection_init_failure) {
    // Create a directory with conflicting name, such that the file
    // creation fails
    std::filesystem::create_directory("tp_samples.txt");
    ASSERT_FALSE(m_archiver->init_sample_collection());
}

// Test if the archiver properly terminates after receiving the given number
// of maximum samples
TEST_F(ArchiverTest, archiver_terminate) {
    int n_blocks = 5;
    auto terminating_archiver
        = std::make_unique<cbdc::archiver::controller>(0,
                                                       m_config_opts,
                                                       m_log,
                                                       n_blocks);

    terminating_archiver->init_leveldb();
    terminating_archiver->init_best_block();
    terminating_archiver->init_sample_collection();

    ASSERT_TRUE(terminating_archiver->running());
    for(int b{0}; b < n_blocks + 1; b++) {
        auto pkt = std::make_shared<cbdc::buffer>();
        auto ser = cbdc::buffer_serializer(*pkt);
        cbdc::atomizer::block blk = m_dummy_blocks[b];
        ser << blk;
        auto msg = cbdc::network::message_t{pkt, 0};
        terminating_archiver->atomizer_handler(std::move(msg));
    }
    ASSERT_FALSE(terminating_archiver->running());
}

// We only test the atomizer initialization in failure, given that
// for a succesful init we need a running atomizer, which by definition
// is an integration test, and as such needs to happen in the integration
// test suite, not here.
TEST_F(ArchiverTest, archiver_atomizer_init_failure) {
    ASSERT_FALSE(m_archiver->init_atomizer_connection());
}

// Test if the archiver properly initializes its server interface
TEST_F(ArchiverTest, archiver_server_init) {
    ASSERT_TRUE(m_archiver->init_archiver_server());
}

TEST_F(ArchiverTest, archiver_server_init_failure) {
    cbdc::network::endpoint_t invalid_endpoint = {"invalid-endpoint", 5000};
    m_config_opts.m_archiver_endpoints.clear();
    m_config_opts.m_archiver_endpoints.emplace_back(invalid_endpoint);
    std::unique_ptr<cbdc::archiver::controller> m_archiver_invalid_endpoint
        = std::make_unique<cbdc::archiver::controller>(0,
                                                       m_config_opts,
                                                       m_log,
                                                       0);
    ASSERT_FALSE(m_archiver_invalid_endpoint->init_archiver_server());
}

// Test if the archiver properly digests a block
TEST_F(ArchiverTest, digest_block) {
    m_archiver->init_leveldb();
    m_archiver->init_best_block();
    m_archiver->digest_block(m_dummy_blocks[0]);
    ASSERT_EQ(m_archiver->best_block_height(), 1);
}

// Test if the archiver properly defers digestion of a block that is received
// out of order
TEST_F(ArchiverTest, digest_block_deferral) {
    m_archiver->init_leveldb();
    m_archiver->init_best_block();
    m_archiver->digest_block(m_dummy_blocks[0]);
    ASSERT_EQ(m_archiver->best_block_height(), 1);
    m_archiver->digest_block(m_dummy_blocks[2]);
    ASSERT_EQ(m_archiver->best_block_height(), 1);
    m_archiver->digest_block(m_dummy_blocks[1]);
    ASSERT_EQ(m_archiver->best_block_height(), 3);
}

// Test the get_block function
TEST_F(ArchiverTest, get_block) {
    m_archiver->init_leveldb();
    m_archiver->init_best_block();
    m_archiver->digest_block(m_dummy_blocks[0]);
    m_archiver->digest_block(m_dummy_blocks[1]);
    m_archiver->digest_block(m_dummy_blocks[2]);
    auto blk = m_archiver->get_block(1);
    ASSERT_TRUE(blk.has_value());
    ASSERT_EQ(blk.value().m_height, 1);
    ASSERT_EQ(blk.value().m_transactions.size(), 20);
    ASSERT_EQ(blk.value().m_transactions[2].m_id,
              m_dummy_blocks[0].m_transactions[2].m_id);
}

// Test the server_handler function
TEST_F(ArchiverTest, server_handler) {
    m_archiver->init_leveldb();
    m_archiver->init_best_block();
    m_archiver->digest_block(m_dummy_blocks[0]);
    m_archiver->digest_block(m_dummy_blocks[1]);
    m_archiver->digest_block(m_dummy_blocks[2]);
    auto pkt = std::make_shared<cbdc::buffer>();
    auto ser = cbdc::buffer_serializer(*pkt);
    ser << static_cast<uint64_t>(1);
    auto msg = cbdc::network::message_t{pkt, 0};
    auto buf = m_archiver->server_handler(std::move(msg));
    ASSERT_TRUE(buf.has_value());
    auto b = buf.value();
    auto deser = cbdc::buffer_serializer(b);
    std::optional<cbdc::atomizer::block> blk;
    deser >> blk;
    ASSERT_TRUE(blk.has_value());
    ASSERT_EQ(blk.value().m_height, 1);
    ASSERT_EQ(blk.value().m_transactions.size(), 20);
    ASSERT_EQ(blk.value().m_transactions[2].m_id,
              m_dummy_blocks[0].m_transactions[2].m_id);
}

// Test the client
TEST_F(ArchiverTest, client) {
    m_archiver->init_leveldb();
    m_archiver->init_best_block();
    ASSERT_TRUE(m_archiver->init_archiver_server());
    m_archiver->digest_block(m_dummy_blocks[0]);
    m_archiver->digest_block(m_dummy_blocks[1]);
    m_archiver->digest_block(m_dummy_blocks[2]);

    auto client
        = cbdc::archiver::client(m_config_opts.m_archiver_endpoints[0], m_log);
    ASSERT_TRUE(client.init());
    auto blk = client.get_block(1);
    ASSERT_TRUE(blk.has_value());
    ASSERT_EQ(blk.value().m_height, 1);
    ASSERT_EQ(blk.value().m_transactions.size(), 20);
    ASSERT_EQ(blk.value().m_transactions[2].m_id,
              m_dummy_blocks[0].m_transactions[2].m_id);
}

// Test if the archiver returns null for a non existent block
TEST_F(ArchiverTest, get_block_non_existent) {
    m_archiver->init_leveldb();
    m_archiver->init_best_block();
    m_archiver->digest_block(m_dummy_blocks[0]);
    m_archiver->digest_block(m_dummy_blocks[1]);
    m_archiver->digest_block(m_dummy_blocks[2]);
    auto blk = m_archiver->get_block(12);
    ASSERT_FALSE(blk.has_value());
}

// Test if the archiver is functional after calling the main init function
TEST_F(ArchiverTest, init) {
    // init should return false because we can't connect to an atomizer
    // but it should still be functional given that the local initialization
    // (level db, block height, sample collection) is done first.
    ASSERT_FALSE(m_archiver->init());
    m_archiver->digest_block(m_dummy_blocks[0]);
    auto blk = m_archiver->get_block(1);
    ASSERT_TRUE(blk.has_value());
    ASSERT_EQ(m_archiver->best_block_height(), 1);
}
