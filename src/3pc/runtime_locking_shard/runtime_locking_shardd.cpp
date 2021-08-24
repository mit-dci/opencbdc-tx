// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"
#include "util.hpp"
#include "util/common/logging.hpp"

#include <csignal>

auto main(int argc, char** argv) -> int {
    auto log
        = std::make_shared<cbdc::logging::log>(cbdc::logging::log_level::warn);
    auto cfg = cbdc::threepc::read_config(argc, argv);
    if(!cfg.has_value()) {
        log->error("Error parsing options");
        return 1;
    }
    log->set_loglevel(cfg->m_loglevel);

    std::shared_ptr<cbdc::telemetry> tel = nullptr;
    if(cfg->m_enable_telemetry) {
        tel = std::make_shared<cbdc::telemetry>("telemetry.bin");
    }

    if(cfg->m_shard_endpoints.size() <= cfg->m_component_id) {
        log->error("No endpoint for component id");
        return 1;
    }

    if(cfg->m_shard_endpoints[cfg->m_component_id].size() <= *cfg->m_node_id) {
        log->error("No endpoint for node id");
        return 1;
    }

    auto raft_endpoints = std::vector<cbdc::network::endpoint_t>();
    for(auto& e : cfg->m_shard_endpoints[cfg->m_component_id]) {
        auto new_ep = e;
        new_ep.second++;
        raft_endpoints.push_back(new_ep);
    }

    auto controller = cbdc::threepc::runtime_locking_shard::controller(
        cfg->m_component_id,
        *cfg->m_node_id,
        cfg->m_shard_endpoints[cfg->m_component_id][*cfg->m_node_id],
        raft_endpoints,
        log,
        tel);
    if(!controller.init()) {
        log->error("Failed to start raft server");
        return 1;
    }

    static auto running = std::atomic_bool{true};

    std::signal(SIGINT, [](int /* signal */) {
        running = false;
    });

    log->info("Shard running");

    while(running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    log->info("Shutting down...");
    return 0;
}
