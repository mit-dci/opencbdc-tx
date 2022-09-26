// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mock_system.hpp"

#include <gtest/gtest.h>
#include <utility>

namespace cbdc::test {
    auto mock_system_module_string(mock_system_module mod) -> std::string {
        switch(mod) {
            case mock_system_module::watchtower:
                return "watchtower";
            case mock_system_module::atomizer:
                return "atomizer";
            case mock_system_module::coordinator:
                return "coordinator";
            case mock_system_module::archiver:
                return "archiver";
            case mock_system_module::shard:
                return "shard";
            case mock_system_module::sentinel:
                return "sentinel";
        }
        return "unknown";
    }

    mock_system::mock_system(
        const std::unordered_set<mock_system_module>& disabled_modules,
        config::options opts)
        : m_opts(std::move(opts)) {
        m_module_endpoints.insert({mock_system_module::watchtower,
                                   m_opts.m_watchtower_internal_endpoints});

        m_module_endpoints.insert(
            {mock_system_module::atomizer, m_opts.m_atomizer_endpoints});

        auto coord_eps = std::vector<network::endpoint_t>();
        for(const auto& node_eps : m_opts.m_coordinator_endpoints) {
            coord_eps.insert(coord_eps.end(),
                             node_eps.begin(),
                             node_eps.end());
        }
        m_module_endpoints.insert(
            {mock_system_module::coordinator, coord_eps});

        m_module_endpoints.insert(
            {mock_system_module::archiver, m_opts.m_archiver_endpoints});

        m_module_endpoints.insert(
            {mock_system_module::shard, m_opts.m_shard_endpoints});

        m_module_endpoints.insert(
            {mock_system_module::sentinel, m_opts.m_sentinel_endpoints});

        for(const auto& m : disabled_modules) {
            m_module_endpoints.erase(m);
        }

        for(auto& module_and_endpoints : m_module_endpoints) {
            const auto& endpoints = module_and_endpoints.second;
            if(endpoints.empty()) {
                const auto& module = module_and_endpoints.first;
                m_logger->warn(
                    "The " + mock_system_module_string(module)
                    + " module was selected to be mocked,\n"
                      "but can't be because no endpoint was given.");
                m_module_endpoints.erase(module);
            }
        }
    }

    mock_system::~mock_system() {
        for(auto& module : m_networks) {
            for(auto& network : module.second) {
                network->close();
            }
        }

        for(auto& t : m_server_handlers) {
            t.join();
        }
    }

    void mock_system::init() {
        for(const auto& module_and_endpoints : m_module_endpoints) {
            const auto& module = module_and_endpoints.first;
            const auto& endpoints = module_and_endpoints.second;
            start_servers(module, endpoints);
        }
    }

    auto mock_system::start_servers(
        const mock_system_module for_module,
        const std::vector<network::endpoint_t>& endpoints) -> bool {
        for(size_t i = 0; i < endpoints.size(); ++i) {
            const auto& ep = endpoints[i];
            auto network
                = std::make_shared<cbdc::network::connection_manager>();
            auto h = network->start_server(
                ep,
                [&, for_module, i](cbdc::network::message_t&& pkt)
                    -> std::optional<cbdc::buffer> {
                    const std::lock_guard<std::mutex> guard(m_handler_lock);
                    auto it = m_expect_handlers.find({for_module, i});
                    if(it == m_expect_handlers.end()) {
                        ADD_FAILURE()
                            << "Unexpected "
                                   + mock_system_module_string(for_module)
                                   + "[" + std::to_string(i) + "]"
                                   + " message received from peer "
                                   + std::to_string(pkt.m_peer_id);
                        return std::nullopt;
                    }
                    auto& [node_id, handler_queue] = *it;
                    if(handler_queue.empty()) {
                        ADD_FAILURE()
                            << "Too many "
                                   + mock_system_module_string(for_module)
                                   + "[" + std::to_string(i) + "]"
                                   + " messages received from peer "
                                   + std::to_string(pkt.m_peer_id);
                        return std::nullopt;
                    }
                    auto fn = handler_queue.front();
                    auto res = fn(std::move(pkt));
                    handler_queue.pop();
                    return res;
                });
            if(!h.has_value()) {
                return false;
            }
            m_networks[for_module].push_back({std::move(network)});
            m_server_handlers.push_back(std::move(h.value()));
        }
        return true;
    }
}
