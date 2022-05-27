// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util/common/hash.hpp"
#include "util/common/logging.hpp"
#include "util/raft/console_logger.hpp"
#include "util/raft/log_store.hpp"
#include "util/raft/messages.hpp"
#include "util/raft/node.hpp"
#include "util/raft/serialization.hpp"
#include "util/raft/state_manager.hpp"
#include "util/raft/util.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/util.hpp"

#include <filesystem>
#include <gtest/gtest.h>

class dummy_sm : public nuraft::state_machine {
  public:
    dummy_sm() = default;

    auto commit(uint64_t log_idx, nuraft::buffer& /* data */)
        -> nuraft::ptr<nuraft::buffer> override {
        m_last_commit_index = log_idx;
        auto resp = true;
        auto buf = nuraft::buffer::alloc(cbdc::serialized_size(resp));
        auto ser = cbdc::nuraft_serializer(*buf);
        ser << resp;
        return buf;
    }

    auto read_logical_snp_obj(nuraft::snapshot& /* s */,
                              void*& /* user_snp_ctx */,
                              uint64_t /* obj_id */,
                              nuraft::ptr<nuraft::buffer>& /* data_out */,
                              bool& /* is_last_obj */) -> int override {
        return 0;
    }

    void save_logical_snp_obj(nuraft::snapshot& /* s */,
                              uint64_t& /* obj_id */,
                              nuraft::buffer& /* data */,
                              bool /* is_first_obj */,
                              bool /* is_last_obj */) override {}

    auto apply_snapshot(nuraft::snapshot& /* s */) -> bool override {
        return true;
    }

    auto last_snapshot() -> nuraft::ptr<nuraft::snapshot> override {
        return m_snapshot;
    }

    auto last_commit_index() -> uint64_t override {
        return m_last_commit_index;
    }

    void create_snapshot(
        nuraft::snapshot& s,
        nuraft::async_result<bool>::handler_type& when_done) override {
        auto snp_buf = s.serialize();
        m_snapshot = nuraft::snapshot::deserialize(*snp_buf);
        nuraft::ptr<std::exception> except(nullptr);
        bool ret = true;
        when_done(ret, except);
    }

  private:
    uint64_t m_last_commit_index = 0;
    nuraft::ptr<nuraft::snapshot> m_snapshot{};
};

class raft_test : public ::testing::Test {
  protected:
    void SetUp() override {
        std::filesystem::remove_all(m_db_dir);
        std::filesystem::remove_all(m_config_file);
        std::filesystem::remove_all(m_state_file);
        for(size_t i{0}; i < 20; i++) {
            auto test_hash
                = cbdc::hash_from_hex("cb7b43951ffcfe400a5432749a79096e632ef2"
                                      "e6328a28049c9af55b85fb260d");
            auto new_log
                = nuraft::buffer::alloc(cbdc::serialized_size(test_hash));
            auto ser = cbdc::nuraft_serializer(*new_log);
            ser << test_hash;
            m_dummy_log_entries.push_back(
                nuraft::cs_new<nuraft::log_entry>(200 + i, new_log));
        }
        m_raft_params.election_timeout_lower_bound_ = 1500;
        m_raft_params.election_timeout_upper_bound_ = 3000;
        m_raft_params.heart_beat_interval_ = 1000;
        m_raft_params.snapshot_distance_ = 0;
        m_raft_params.max_append_size_ = 100000;
        m_raft_endpoints.emplace_back("127.0.0.1", 5000);
        m_raft_endpoints.emplace_back("127.0.0.1", 5001);
        m_raft_endpoints.emplace_back("127.0.0.1", 5002);
    }

    void TearDown() override {
        std::filesystem::remove_all(m_db_dir);
        std::filesystem::remove_all(m_config_file);
        std::filesystem::remove_all(m_state_file);
        for(size_t i{0}; i < m_raft_endpoints.size(); i++) {
            std::filesystem::remove_all("test_raft_config_" + std::to_string(i)
                                        + ".dat");
            std::filesystem::remove_all("test_raft_state_" + std::to_string(i)
                                        + ".dat");
            std::filesystem::remove_all("test_raft_log_" + std::to_string(i));
        }
        std::filesystem::remove_all(m_log_file);
    }

    void basic_raft_cluster_test(bool blocking) {
        auto log = std::make_shared<cbdc::logging::log>(
            cbdc::logging::log_level::trace,
            false,
            std::unique_ptr<std::ostream>(
                new std::ofstream(m_log_file,
                                  std::ios::out | std::ios::trunc)));

        auto nodes = std::vector<std::unique_ptr<cbdc::raft::node>>();
        auto sms = std::vector<std::shared_ptr<dummy_sm>>();
        for(size_t i{0}; i < m_raft_endpoints.size(); i++) {
            auto sm = std::make_shared<dummy_sm>();
            nodes.emplace_back(
                std::make_unique<cbdc::raft::node>(static_cast<int>(i),
                                                   m_raft_endpoints[i],
                                                   "test",
                                                   blocking,
                                                   sm,
                                                   10,
                                                   log,
                                                   nullptr,
                                                   false));
            sms.emplace_back(sm);
        }

        auto init_threads = std::vector<std::thread>(m_raft_endpoints.size());
        for(size_t i{1}; i < m_raft_endpoints.size(); i++) {
            std::thread t(
                [&](cbdc::raft::node& node) {
                    node.init(m_raft_params);
                    node.build_cluster(m_raft_endpoints);
                },
                std::ref(*nodes[i]));

            init_threads[i] = std::move(t);
        }

        std::thread t(
            [&](cbdc::raft::node& node) {
                node.init(m_raft_params);
                node.build_cluster(m_raft_endpoints);
            },
            std::ref(*nodes[0]));

        init_threads[0] = std::move(t);

        for(auto& thr : init_threads) {
            thr.join();
        }

        ASSERT_TRUE(nodes[0]->is_leader());
        ASSERT_FALSE(nodes[1]->is_leader());
        ASSERT_FALSE(nodes[2]->is_leader());
        ASSERT_EQ(nodes[0]->last_log_idx(), 0UL);
        ASSERT_EQ(nodes[1]->last_log_idx(), 0UL);
        ASSERT_EQ(nodes[2]->last_log_idx(), 0UL);

        auto new_log
            = cbdc::make_buffer<uint64_t, nuraft::ptr<nuraft::buffer>>(1);

        auto res = nodes[0]->replicate_sync(new_log);
        ASSERT_TRUE(res.has_value());
        ASSERT_EQ(nodes[0]->last_log_idx(), 4UL);

        cbdc::raft::callback_type result_fn = nullptr;
        auto result_done = std::atomic<bool>(false);
        if(!blocking) {
            result_fn = [&](cbdc::raft::result_type& r,
                            nuraft::ptr<std::exception>& err) {
                ASSERT_FALSE(err);
                const auto raft_result = r.get();
                ASSERT_TRUE(raft_result);
                result_done = true;
            };
        } else {
            result_done = true;
        }
        ASSERT_TRUE(nodes[0]->replicate(new_log, result_fn));
        while(!result_done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        ASSERT_EQ(nodes[0]->last_log_idx(), 5UL);

        for(size_t i{0}; i < nodes.size(); i++) {
            ASSERT_EQ(nodes[i]->get_sm(), sms[i].get());
        }
    }

    static constexpr const auto m_db_dir = "test_db";
    static constexpr const auto m_config_file = "config_file";
    static constexpr const auto m_state_file = "state_file";
    static constexpr const auto m_endpoint = "endpoint";
    std::vector<nuraft::ptr<nuraft::log_entry>> m_dummy_log_entries;
    static constexpr const auto m_log_file = "log_file";
    nuraft::raft_params m_raft_params{};
    std::vector<cbdc::network::endpoint_t> m_raft_endpoints{};
};

TEST_F(raft_test, test_init) {
    auto log_store = cbdc::raft::log_store();
    ASSERT_TRUE(log_store.load(m_db_dir));
    ASSERT_EQ(log_store.next_slot(), 1U);
    ASSERT_EQ(log_store.start_index(), 1U);
    auto last_entry = log_store.last_entry();
    ASSERT_TRUE(last_entry);

    // Check for null entry at first/last position
    ASSERT_EQ(last_entry->get_term(), 0U);
    ASSERT_TRUE(last_entry->is_buf_null());
}

TEST_F(raft_test, test_state_manager_store_and_read_state) {
    auto sm = cbdc::raft::state_manager(3,
                                        m_endpoint,
                                        m_db_dir,
                                        m_config_file,
                                        m_state_file);
    ASSERT_EQ(sm.server_id(), 3);

    auto state = nuraft::srv_state(100, 10, true);
    sm.save_state(state);
    auto loaded_state = sm.read_state();
    ASSERT_EQ(loaded_state->get_term(), 100UL);
    ASSERT_EQ(loaded_state->get_voted_for(), 10);
    ASSERT_TRUE(loaded_state->is_election_timer_allowed());
}

TEST_F(raft_test, test_state_manager_fail_read) {
    auto sm = cbdc::raft::state_manager(0,
                                        m_endpoint,
                                        m_db_dir,
                                        m_config_file,
                                        "non-existent-state");
    auto state = sm.read_state();
    ASSERT_EQ(state, nullptr);
}

TEST_F(raft_test, test_state_manager_store_and_read_config) {
    auto sm = cbdc::raft::state_manager(3,
                                        m_endpoint,
                                        m_db_dir,
                                        m_config_file,
                                        m_state_file);

    auto cfg = nuraft::cluster_config(100, 10, true);
    auto srv_config = nuraft::cs_new<nuraft::srv_config>(0, "endpoint2");
    cfg.get_servers().push_back(srv_config);
    sm.save_config(cfg);
    auto loaded_cfg = sm.load_config();
    ASSERT_EQ(loaded_cfg->get_log_idx(), 100UL);
    ASSERT_EQ(loaded_cfg->get_prev_log_idx(), 10UL);
    ASSERT_TRUE(loaded_cfg->is_async_replication());
    ASSERT_EQ(loaded_cfg->get_servers().size(), 1UL);
    ASSERT_EQ(loaded_cfg->get_server(0)->get_endpoint(),
              "endpoint2"); // m_endpoint would be the default
}

TEST_F(raft_test, test_state_manager_default_config) {
    auto sm = cbdc::raft::state_manager(0,
                                        m_endpoint,
                                        m_db_dir,
                                        "non-existent-config",
                                        m_state_file);
    auto cfg = sm.load_config();
    ASSERT_EQ(cfg->get_log_idx(), 0UL);
    ASSERT_EQ(cfg->get_prev_log_idx(), 0UL);
    ASSERT_FALSE(cfg->is_async_replication());
    ASSERT_EQ(cfg->get_servers().size(), 1UL);
    ASSERT_EQ(cfg->get_server(0)->get_endpoint(), m_endpoint);
}

TEST_F(raft_test, test_state_manager_load_logstore) {
    auto sm = cbdc::raft::state_manager(0,
                                        m_endpoint,
                                        m_db_dir,
                                        m_config_file,
                                        m_state_file);
    auto ls = sm.load_log_store();
    ASSERT_NE(ls, nullptr);
}

TEST_F(raft_test, test_state_manager_fail_logstore) {
    auto sm = cbdc::raft::state_manager(0,
                                        m_endpoint,
                                        m_db_dir,
                                        m_config_file,
                                        m_state_file);
    std::filesystem::remove_all(m_db_dir);
    // Make a file with the same name as the DB directory
    // That will make the init fail
    auto fs = std::ofstream(m_db_dir, std::ios::out);
    fs.close();
    auto ls = sm.load_log_store();
    ASSERT_EQ(ls, nullptr);
}

TEST_F(raft_test, test_raft_serializer_basic) {
    auto new_log = nuraft::buffer::alloc(2);
    auto ser = cbdc::nuraft_serializer(*new_log);
    ASSERT_TRUE(ser);
    ASSERT_FALSE(ser.end_of_buffer());
    ser.advance_cursor(1);
    ASSERT_FALSE(ser.end_of_buffer());
    ser.advance_cursor(2);
    ASSERT_TRUE(ser.end_of_buffer());
}

TEST_F(raft_test, test_raft_serializer_read) {
    auto new_log = nuraft::buffer::alloc(32);
    auto test_hash = cbdc::hash_from_hex(
        "cb7b43951ffcfe400a5432749a79096e632ef2e6328a28049c9af55b85fb260d");
    auto ser = cbdc::nuraft_serializer(*new_log);
    ser << test_hash;
    ser.reset();
    ASSERT_FALSE(ser.end_of_buffer());
    auto read_output = nuraft::buffer::alloc(32);
    ASSERT_TRUE(ser.read(read_output->data(), 32));
    ASSERT_EQ(*read_output->data(), *test_hash.data());
    ser.reset();
    // Test advance_cursor method with read to follow as well.
    ser.advance_cursor(10);
    ASSERT_TRUE(ser.read(read_output->data(), 22));
    ASSERT_EQ(*read_output->data(), *(test_hash.data() + 10));
}

TEST_F(raft_test, test_raft_serializer_out_of_bounds) {
    auto new_log = nuraft::buffer::alloc(32);
    auto test_hash = cbdc::hash_from_hex(
        "cb7b43951ffcfe400a5432749a79096e632ef2e6328a28049c9af55b85fb260d");
    auto ser = cbdc::nuraft_serializer(*new_log);
    ser << test_hash;
    ser.reset();
    ASSERT_FALSE(ser.end_of_buffer());
    ser.advance_cursor(32);
    ASSERT_TRUE(ser.end_of_buffer());
    ASSERT_FALSE(ser.read(nullptr, 10));
    ASSERT_FALSE(ser);
    ASSERT_FALSE(ser.write(nullptr, 10));
}

TEST_F(raft_test, serialize_nuraft_buffer) {
    auto test_hash = cbdc::hash_from_hex(
        "cb7b43951ffcfe400a5432749a79096e632ef2e6328a28049c9af55b85fb260d");
    auto new_log = nuraft::buffer::alloc(cbdc::serialized_size(test_hash));
    auto ser = cbdc::nuraft_serializer(*new_log);
    ser << test_hash;

    auto buf = cbdc::buffer();
    auto ser2 = cbdc::buffer_serializer(buf);
    ser2 << new_log;
    ASSERT_EQ(buf.to_hex(),
              "cb7b43951ffcfe400a5432749a79096e632ef2e6328a280"
              "49c9af55b85fb260d");
}

TEST_F(raft_test, log_store_append) {
    auto log_store = cbdc::raft::log_store();
    ASSERT_TRUE(log_store.load(m_db_dir));
    ASSERT_EQ(log_store.append(m_dummy_log_entries[0]), 1UL);
    ASSERT_EQ(log_store.append(m_dummy_log_entries[1]), 2UL);
}

TEST_F(raft_test, log_store_load_filled) {
    {
        auto log_store = cbdc::raft::log_store();
        ASSERT_TRUE(log_store.load(m_db_dir));

        for(auto& entry : m_dummy_log_entries) {
            log_store.append(entry);
        }
    }
    {
        auto log_store2 = cbdc::raft::log_store();
        ASSERT_TRUE(log_store2.load(m_db_dir));
        ASSERT_EQ(log_store2.next_slot(), m_dummy_log_entries.size() + 1);
        ASSERT_EQ(log_store2.start_index(), 1UL);

        auto entry = log_store2.last_entry();
        auto last_dummy_entry = m_dummy_log_entries.back();
        ASSERT_EQ(entry->get_term(), last_dummy_entry->get_term());
        ASSERT_EQ(std::memcmp(entry->serialize()->data_begin(),
                              last_dummy_entry->serialize()->data_begin(),
                              entry->serialize()->size()),
                  0);
    }
}

TEST_F(raft_test, log_store_get_range) {
    auto log_store = cbdc::raft::log_store();
    ASSERT_TRUE(log_store.load(m_db_dir));

    for(auto& entry : m_dummy_log_entries) {
        log_store.append(entry);
    }

    auto log_range = log_store.log_entries(5, 10);

    size_t i{4};
    for(const auto& entry : *log_range) {
        ASSERT_EQ(entry->get_term(), m_dummy_log_entries[i]->get_term());
        ASSERT_EQ(
            std::memcmp(entry->serialize()->data_begin(),
                        m_dummy_log_entries[i]->serialize()->data_begin(),
                        entry->serialize()->size()),
            0);
        i++;
    }
}

TEST_F(raft_test, log_store_write_at) {
    auto log_store = cbdc::raft::log_store();
    ASSERT_TRUE(log_store.load(m_db_dir));

    for(auto& entry : m_dummy_log_entries) {
        log_store.append(entry);
    }

    ASSERT_EQ(log_store.next_slot(), m_dummy_log_entries.size() + 1);
    log_store.write_at(3, m_dummy_log_entries[2]);
    ASSERT_EQ(log_store.next_slot(), 4UL);

    // Try to get the erased entry - should return null
    auto entry = log_store.entry_at(4);
    ASSERT_EQ(entry->get_term(), 0U);
    ASSERT_TRUE(entry->is_buf_null());
}

TEST_F(raft_test, log_store_pack_apply) {
    auto log_store = cbdc::raft::log_store();
    ASSERT_TRUE(log_store.load(m_db_dir));

    for(auto& entry : m_dummy_log_entries) {
        log_store.append(entry);
    }

    ASSERT_EQ(log_store.next_slot(), m_dummy_log_entries.size() + 1);
    auto pack = log_store.pack(4, 17);
    log_store.write_at(3, m_dummy_log_entries[2]);
    ASSERT_EQ(log_store.next_slot(), 4UL);

    log_store.apply_pack(4, *pack);
    ASSERT_EQ(log_store.next_slot(), m_dummy_log_entries.size() + 1);

    auto entry = log_store.entry_at(m_dummy_log_entries.size());
    ASSERT_EQ(entry->get_term(), m_dummy_log_entries.back()->get_term());
    ASSERT_EQ(
        std::memcmp(entry->serialize()->data_begin(),
                    m_dummy_log_entries.back()->serialize()->data_begin(),
                    entry->serialize()->size()),
        0);
}

TEST_F(raft_test, log_store_flush) {
    auto log_store = cbdc::raft::log_store();
    ASSERT_TRUE(log_store.load(m_db_dir));

    for(auto& entry : m_dummy_log_entries) {
        log_store.append(entry);
    }
    ASSERT_TRUE(log_store.flush());
}

TEST_F(raft_test, log_store_compact) {
    {
        auto log_store = cbdc::raft::log_store();
        ASSERT_TRUE(log_store.load(m_db_dir));

        for(auto& entry : m_dummy_log_entries) {
            log_store.append(entry);
        }
        ASSERT_TRUE(log_store.compact(16));
    }
    {
        auto log_store2 = cbdc::raft::log_store();
        ASSERT_TRUE(log_store2.load(m_db_dir));
        ASSERT_EQ(log_store2.next_slot(), m_dummy_log_entries.size() + 1);
        ASSERT_EQ(log_store2.start_index(), 17UL);
    }
}

TEST_F(raft_test, log_store_term_at) {
    auto log_store = cbdc::raft::log_store();
    ASSERT_TRUE(log_store.load(m_db_dir));

    for(auto& entry : m_dummy_log_entries) {
        log_store.append(entry);
    }

    size_t i{1};
    for(auto& entry : m_dummy_log_entries) {
        ASSERT_EQ(log_store.term_at(i), entry->get_term());
        i++;
    }
}

TEST_F(raft_test, console_logger_loglevel) {
    // TODO: split these tests into separate fixtures.
    {
        auto log = std::make_shared<cbdc::logging::log>(
            cbdc::logging::log_level::trace);
        auto raft_log = cbdc::raft::console_logger(log);
        ASSERT_EQ(raft_log.get_level(),
                  static_cast<int>(cbdc::raft::log_level::trace));
    }
    {
        auto log = std::make_shared<cbdc::logging::log>(
            cbdc::logging::log_level::debug);
        auto raft_log = cbdc::raft::console_logger(log);
        ASSERT_EQ(raft_log.get_level(),
                  static_cast<int>(cbdc::raft::log_level::debug));
    }
    {
        auto log = std::make_shared<cbdc::logging::log>(
            cbdc::logging::log_level::info);
        auto raft_log = cbdc::raft::console_logger(log);
        ASSERT_EQ(raft_log.get_level(),
                  static_cast<int>(cbdc::raft::log_level::info));
    }
    {
        auto log = std::make_shared<cbdc::logging::log>(
            cbdc::logging::log_level::warn);
        auto raft_log = cbdc::raft::console_logger(log);
        ASSERT_EQ(raft_log.get_level(),
                  static_cast<int>(cbdc::raft::log_level::warn));
    }
    {
        auto log = std::make_shared<cbdc::logging::log>(
            cbdc::logging::log_level::error);
        auto raft_log = cbdc::raft::console_logger(log);
        ASSERT_EQ(raft_log.get_level(),
                  static_cast<int>(cbdc::raft::log_level::error));
    }
    {
        auto log = std::make_shared<cbdc::logging::log>(
            cbdc::logging::log_level::fatal);
        auto raft_log = cbdc::raft::console_logger(log);
        ASSERT_EQ(raft_log.get_level(),
                  static_cast<int>(cbdc::raft::log_level::error));
    }
}

TEST_F(raft_test, console_logger) {
    {
        auto log = std::make_shared<cbdc::logging::log>(
            cbdc::logging::log_level::trace,
            false,
            std::unique_ptr<std::ostream>(
                new std::ofstream(m_log_file,
                                  std::ios::out | std::ios::trunc)));

        auto raft_log = cbdc::raft::console_logger(log);

        ASSERT_EQ(raft_log.get_level(),
                  static_cast<int>(cbdc::raft::log_level::trace));

        raft_log.put_details(static_cast<int>(cbdc::raft::log_level::trace),
                             "test_file",
                             "test_func",
                             100,
                             "test_log_trace");
        raft_log.put_details(static_cast<int>(cbdc::raft::log_level::debug),
                             "test_file2",
                             "test_func2",
                             200,
                             "test_log_debug");
        raft_log.put_details(static_cast<int>(cbdc::raft::log_level::info),
                             "test_file3",
                             "test_func3",
                             300,
                             "test_log_info");
        raft_log.put_details(static_cast<int>(cbdc::raft::log_level::warn),
                             "test_file4",
                             "test_func4",
                             400,
                             "test_log_warn");
        raft_log.put_details(static_cast<int>(cbdc::raft::log_level::error),
                             "test_file5",
                             "test_func5",
                             500,
                             "test_log_error");
        raft_log.put_details(static_cast<int>(cbdc::raft::log_level::fatal),
                             "test_file6",
                             "test_func6",
                             600,
                             "test_log_fatal");
    }
    {
        std::ifstream t(m_log_file);
        std::string str;

        t.seekg(0, std::ios::end);
        str.reserve(t.tellg());
        t.seekg(0, std::ios::beg);

        str.assign((std::istreambuf_iterator<char>(t)),
                   std::istreambuf_iterator<char>());

        auto idx
            = str.find("[TRACE] test_file : 100 test_func test_log_trace\n");
        ASSERT_NE(idx, std::string::npos);
        auto idx2
            = str.find("[DEBUG] test_file2 : 200 test_func2 test_log_debug\n",
                       idx);
        ASSERT_NE(idx2, std::string::npos);
        ASSERT_TRUE(idx2 > idx);
        idx = idx2;
        idx2 = str.find("[INFO ] test_file3 : 300 test_func3 test_log_info\n",
                        idx);
        ASSERT_NE(idx2, std::string::npos);
        ASSERT_TRUE(idx2 > idx);
        idx = idx2;
        idx2 = str.find("[WARN ] test_file4 : 400 test_func4 test_log_warn\n",
                        idx);
        ASSERT_NE(idx2, std::string::npos);
        ASSERT_TRUE(idx2 > idx);
        idx = idx2;
        idx2 = str.find("[ERROR] test_file5 : 500 test_func5 test_log_error\n",
                        idx);
        ASSERT_NE(idx2, std::string::npos);
        ASSERT_TRUE(idx2 > idx);
        idx = idx2;
        idx2 = str.find("[ERROR] test_file6 : 600 test_func6 test_log_fatal\n",
                        idx);
        ASSERT_NE(idx2, std::string::npos);
        ASSERT_TRUE(idx2 > idx);
    }
}

TEST_F(raft_test, raft_node_test_blocking) {
    basic_raft_cluster_test(true);
}

TEST_F(raft_test, raft_node_test_non_blocking) {
    basic_raft_cluster_test(false);
}

TEST_F(raft_test, index_comparator_test) {
    auto cmp = cbdc::raft::index_comparator();
    auto str = std::string("hello!");
    auto orig_str = str;
    auto str_for_slice = std::string("hi :)");
    auto slice = leveldb::Slice(str_for_slice.c_str());
    cmp.FindShortestSeparator(&str, slice);
    ASSERT_EQ(str, orig_str);
    cmp.FindShortSuccessor(&str);
    ASSERT_EQ(str, orig_str);
}

TEST_F(raft_test, test_state_manager_exit) {
    auto sm = cbdc::raft::state_manager(0,
                                        m_endpoint,
                                        m_db_dir,
                                        m_config_file,
                                        m_state_file);
    ASSERT_EXIT(sm.system_exit(20), ::testing::ExitedWithCode(20), "c*");
}

TEST(raft_buffer_serialization_test, make_buffer) {
    constexpr uint64_t obj_to_serialize = std::numeric_limits<uint64_t>::max();
    const auto buf = cbdc::make_buffer<uint64_t, nuraft::ptr<nuraft::buffer>>(
        obj_to_serialize);

    const auto obj_size = cbdc::serialized_size(obj_to_serialize);
    ASSERT_EQ(buf->size(), obj_size);
    // For the num. of buffer metadata bytes, see nuraft::buffer::alloc()
    // in buffer.cxx in the source code for NuRaft-1.3.0.  The expression below
    // is valid for buffer sizes <= 32kB.
    constexpr uint64_t num_buf_metadata_bytes = sizeof(ushort) * 2;
    const uint64_t expected_buf_capacity = obj_size + num_buf_metadata_bytes;
    ASSERT_EQ(buf->container_size(), expected_buf_capacity);

    ASSERT_EQ(buf->pos(), obj_size);

    buf->pos(0);
    ASSERT_EQ(buf->get_ulong(), obj_to_serialize);
}

TEST(raft_buffer_serialization_test, from_buffer) {
    // Test successful deserialization.
    constexpr uint64_t obj_to_serialize = std::numeric_limits<uint64_t>::max();
    const auto buf = cbdc::make_buffer<uint64_t, nuraft::ptr<nuraft::buffer>>(
        obj_to_serialize);
    const auto deser_obj = cbdc::from_buffer<uint64_t>(*buf);
    EXPECT_EQ(deser_obj.value(), obj_to_serialize);

    // Test unsuccessful deserialization from an empty NuRaft buffer.
    const auto empty_buf = nuraft::buffer::alloc(0);
    const auto empty_deser_obj = cbdc::from_buffer<uint64_t>(*empty_buf);
    EXPECT_FALSE(empty_deser_obj.has_value());
}
