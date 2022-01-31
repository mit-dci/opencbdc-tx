// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "state_manager.hpp"

#include "log_store.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <utility>

namespace cbdc::raft {
    state_manager::state_manager(int32_t srv_id,
                                 std::string endpoint,
                                 std::string log_dir,
                                 std::string config_file,
                                 std::string state_file)
        : m_id(srv_id),
          m_endpoint(std::move(endpoint)),
          m_config_file(std::move(config_file)),
          m_state_file(std::move(state_file)),
          m_log_dir(std::move(log_dir)) {}

    template<typename T>
    void save_object(const T& obj, const std::string& filename) {
        auto buf = obj.serialize();
        std::vector<char> char_buf(buf->size());
        std::memcpy(char_buf.data(), buf->data_begin(), char_buf.size());

        const auto tmp_file = filename + ".tmp";
        std::ofstream file(tmp_file, std::ios::binary | std::ios::trunc);
        assert(file.good());

        file.write(char_buf.data(),
                   static_cast<std::streamsize>(char_buf.size()));
        file.flush();
        file.close();

        std::filesystem::rename(tmp_file, filename);
    }

    template<typename T>
    auto load_object(const std::string& filename) -> nuraft::ptr<T> {
        std::ifstream file(filename, std::ios::binary);
        if(!file.good()) {
            return nullptr;
        }

        auto file_sz = std::filesystem::file_size(filename);
        std::vector<char> buf(file_sz);
        if(!file.read(buf.data(), static_cast<std::streamsize>(file_sz))) {
            return nullptr;
        }

        auto nuraft_buf = nuraft::buffer::alloc(file_sz);
        std::memcpy(nuraft_buf->data_begin(), buf.data(), nuraft_buf->size());

        auto obj = T::deserialize(*nuraft_buf);
        return obj;
    }

    auto state_manager::load_config() -> nuraft::ptr<nuraft::cluster_config> {
        auto config = load_object<nuraft::cluster_config>(m_config_file);
        if(!config) {
            auto srv_config
                = nuraft::cs_new<nuraft::srv_config>(m_id, m_endpoint);
            auto cluster_config = nuraft::cs_new<nuraft::cluster_config>();
            cluster_config->get_servers().push_back(srv_config);
            return cluster_config;
        }

        return config;
    }

    void state_manager::save_config(const nuraft::cluster_config& config) {
        save_object(config, m_config_file);
    }

    void state_manager::save_state(const nuraft::srv_state& state) {
        save_object(state, m_state_file);
    }

    auto state_manager::read_state() -> nuraft::ptr<nuraft::srv_state> {
        auto state = load_object<nuraft::srv_state>(m_state_file);
        return state;
    }

    auto state_manager::load_log_store() -> nuraft::ptr<nuraft::log_store> {
        auto log = nuraft::cs_new<log_store>();
        if(!log->load(m_log_dir)) {
            return nullptr;
        }

        return log;
    }

    auto state_manager::server_id() -> int32_t {
        return m_id;
    }

    void state_manager::system_exit(int exit_code) {
        std::exit(exit_code);
    }
}
