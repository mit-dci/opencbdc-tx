// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RAFT_STATE_MANAGER_H_
#define OPENCBDC_TX_SRC_RAFT_STATE_MANAGER_H_

#include "log_store.hpp"

#include <libnuraft/nuraft.hxx>

namespace cbdc::raft {
    /// Implementation of nuraft::state_mgr using a file.
    class state_manager : public nuraft::state_mgr {
      public:
        /// Constructor.
        /// \param srv_id ID of the raft node.
        /// \param endpoint raft endpoint.
        /// \param log_dir directory for the raft log.
        /// \param config_file file for the cluster configuration.
        /// \param state_file file for the server state.
        state_manager(int32_t srv_id,
                      std::string endpoint,
                      std::string log_dir,
                      std::string config_file,
                      std::string state_file);
        ~state_manager() override = default;

        state_manager(const state_manager& other) = delete;
        auto operator=(const state_manager& other) -> state_manager& = delete;

        state_manager(state_manager&& other) noexcept;
        auto operator=(state_manager&& other) noexcept -> state_manager&;

        /// Read and deserialize the cluster configuration.
        /// \return cluster configuration, or nullptr if the file is empty.
        auto load_config() -> nuraft::ptr<nuraft::cluster_config> override;

        /// Serialize and write the given cluster configuration.
        /// \param config cluster configuration.
        void save_config(const nuraft::cluster_config& config) override;

        /// Serialize and write the given server state.
        /// \param state server state.
        void save_state(const nuraft::srv_state& state) override;

        /// Read and deserialize the server state.
        /// \return server state, or nullptr if the file is empty.
        auto read_state() -> nuraft::ptr<nuraft::srv_state> override;

        /// Load and return the log store.
        /// \return log store instance, or nullptr if loading failed.
        auto load_log_store() -> nuraft::ptr<nuraft::log_store> override;

        /// Return the server ID.
        /// \return server ID.
        auto server_id() -> int32_t override;

        /// Terminate the application with the given exit code.
        /// \param exit_code application return code.
        void system_exit(int exit_code) override;

      private:
        int32_t m_id;
        std::string m_endpoint;
        std::string m_config_file;
        std::string m_state_file;
        std::string m_log_dir;
    };
}

#endif // OPENCBDC_TX_SRC_RAFT_STATE_MANAGER_H_
