// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "node.hpp"

namespace cbdc::raft {
    node::node(int node_id,
               const network::endpoint_t& raft_endpoint,
               const std::string& node_type,
               bool blocking,
               nuraft::ptr<nuraft::state_machine> sm,
               size_t asio_thread_pool_size,
               std::shared_ptr<logging::log> logger,
               nuraft::cb_func::func_type raft_cb)
        : m_node_id(static_cast<uint32_t>(node_id)),
          m_blocking(blocking),
          m_port(raft_endpoint.second),
          m_raft_logger(nuraft::cs_new<console_logger>(std::move(logger))),
          m_smgr(nuraft::cs_new<state_manager>(
              static_cast<uint32_t>(m_node_id + 1),
              raft_endpoint.first + ":" + std::to_string(m_port),
              node_type + "_raft_log_" + std::to_string(m_node_id),
              node_type + "_raft_config_" + std::to_string(m_node_id) + ".dat",
              node_type + "_raft_state_" + std::to_string(m_node_id)
                  + ".dat")),
          m_sm(std::move(sm)) {
        m_asio_opt.thread_pool_size_ = asio_thread_pool_size;
        m_init_opts.raft_callback_ = std::move(raft_cb);
        if(m_node_id != 0) {
            m_init_opts.skip_initial_election_timeout_ = true;
        }
    }

    auto node::init(const nuraft::raft_params& raft_params) -> bool {
        // Set these explicitly as they depend on the structure of this class
        auto params = raft_params;
        if(m_blocking) {
            params.return_method_ = nuraft::raft_params::blocking;
        } else {
            params.return_method_ = nuraft::raft_params::async_handler;
        }
        params.auto_forwarding_ = false;

        m_raft_instance = m_launcher.init(m_sm,
                                          m_smgr,
                                          m_raft_logger,
                                          m_port,
                                          m_asio_opt,
                                          params,
                                          m_init_opts);

        if(!m_raft_instance) {
            std::cerr << "Failed to initialize raft launcher" << std::endl;
            return false;
        }

        std::cout << "Waiting for raft initialization";
        static constexpr auto wait_time = std::chrono::milliseconds(100);
        while(!m_raft_instance->is_initialized()) {
            std::cout << "." << std::flush;
            std::this_thread::sleep_for(wait_time);
        }
        std::cout << std::endl;

        return true;
    }

    auto node::add_cluster_nodes(
        const std::vector<network::endpoint_t>& raft_servers) const -> bool {
        static constexpr auto sleep_time = std::chrono::milliseconds(100);

        auto srvs = std::vector<std::pair<int, std::string>>();
        for(size_t i{0}; i < raft_servers.size(); i++) {
            if(i != static_cast<size_t>(m_node_id)) {
                const auto& ep = raft_servers[i];
                auto ep_str = ep.first + ":" + std::to_string(ep.second);
                srvs.emplace_back(std::make_pair(i + 1, ep_str));
            }
        }

        for(const auto& srv_data : srvs) {
            nuraft::srv_config srv(srv_data.first, srv_data.second);
            std::cout << "Adding raft server: " << srv.get_id() << ", "
                      << srv.get_endpoint() << std::flush;

            auto ret = m_raft_instance->add_srv(srv);
            if(!ret->get_accepted()) {
                std::cout << "Failed to add raft server: " << srv.get_id()
                          << ", " << srv.get_endpoint()
                          << ", error: " << ret->get_result_str() << std::endl;
                return false;
            }

            nuraft::ptr<nuraft::srv_config> srv_conf;
            int attempts{0};
            const auto max_retries = 200;
            do {
                srv_conf = m_raft_instance->get_srv_config(srv_data.first);
                std::cout << "." << std::flush;
                attempts++;
                std::this_thread::sleep_for(sleep_time);
            } while(!srv_conf && attempts < max_retries);

            if(!srv_conf) {
                std::cout << "timed out" << std::endl;
                return false;
            }

            std::cout << "done" << std::endl;
        }

        return true;
    }

    auto
    node::build_cluster(const std::vector<network::endpoint_t>& raft_servers)
        -> bool {
        std::vector<nuraft::ptr<nuraft::srv_config>> srv_configs;
        m_raft_instance->get_srv_config_all(srv_configs);

        static constexpr auto sleep_time = std::chrono::milliseconds(100);

        if(srv_configs.size() < raft_servers.size()) {
            if(m_node_id != 0) {
                std::cout << "Waiting for raft cluster";
                do {
                    srv_configs.clear();
                    m_raft_instance->get_srv_config_all(srv_configs);
                    std::cout << "." << std::flush;
                    std::this_thread::sleep_for(sleep_time);
                } while(srv_configs.size() < raft_servers.size());
                std::cout << "done" << std::endl;
            } else if(!add_cluster_nodes(raft_servers)) {
                return false;
            }
        }

        m_raft_instance->restart_election_timer();

        return true;
    }

    auto node::is_leader() const -> bool {
        return m_raft_instance->is_leader();
    }

    auto node::replicate(nuraft::ptr<nuraft::buffer> new_log,
                         const callback_type& result_fn) const -> bool {
        auto ret = m_raft_instance->append_entries({std::move(new_log)});
        if(!ret->get_accepted()) {
            return false;
        }

        if(result_fn) {
            ret->when_ready(result_fn);
        }

        return true;
    }

    auto node::replicate_sync(const nuraft::ptr<nuraft::buffer>& new_log) const
        -> std::optional<nuraft::ptr<nuraft::buffer>> {
        auto ret = m_raft_instance->append_entries({new_log});
        if(!ret->get_accepted()
           || ret->get_result_code() != nuraft::cmd_result_code::OK) {
            return std::nullopt;
        }

        return ret->get();
    }

    node::~node() {
        stop();
    }

    auto node::last_log_idx() const -> uint64_t {
        return m_sm->last_commit_index();
    }

    auto node::get_sm() const -> nuraft::state_machine* {
        return m_sm.get();
    }

    void node::stop() {
        m_launcher.shutdown();
    }
}
