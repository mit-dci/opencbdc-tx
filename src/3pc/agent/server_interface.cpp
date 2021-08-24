// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "server_interface.hpp"

#include <cassert>

namespace cbdc::threepc::agent::rpc {
    server_interface::server_interface(
        std::shared_ptr<broker::interface> broker,
        std::shared_ptr<logging::log> log,
        std::shared_ptr<cbdc::telemetry> tel,
        const cbdc::threepc::config& cfg)
        : m_broker(std::move(broker)),
          m_log(std::move(log)),
          m_tel(std::move(tel)),
          m_cfg(cfg) {
        m_cleanup_thread = std::thread([&]() {
            size_t id{};
            while(m_cleanup_queue.pop(id)) {
                std::unique_lock l(m_agents_mut);
                m_agents.erase(id);
            }
        });
        m_retry_thread = std::thread([&]() {
            size_t id{};
            while(m_retry_queue.pop(id)) {
                auto a = [&]() {
                    std::unique_lock l(m_agents_mut);
                    auto it = m_agents.find(id);
                    assert(it != m_agents.end());
                    return it->second;
                }();
                if(!a->exec()) {
                    m_log->fatal("Error retrying agent");
                }
            }
        });
    }

    server_interface::~server_interface() {
        m_retry_queue.clear();
        m_retry_thread.join();
        m_log->trace("Stopped retry thread");
        m_cleanup_queue.clear();
        m_cleanup_thread.join();
        m_log->trace("Stopped runner cleanup thread");
        {
            std::unique_lock l(m_agents_mut);
            m_agents.clear();
        }
        m_log->trace("Cleaned up all runners");
    }
}
