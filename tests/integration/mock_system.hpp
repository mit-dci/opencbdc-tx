// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * \file mock_system.hpp
 * Dummy network capabilities to support integration testing.
 */

#ifndef OPENCBDC_TX_TESTS_INTEGRATION_MOCK_SYSTEM_H_
#define OPENCBDC_TX_TESTS_INTEGRATION_MOCK_SYSTEM_H_

#include "util/common/config.hpp"
#include "util/network/connection_manager.hpp"
#include "util/serialization/buffer_serializer.hpp"
#include "util/serialization/format.hpp"

#include <future>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace cbdc::test {
    /// Enumeration of the components the mock system can simulate.
    enum class mock_system_module : uint8_t {
        watchtower,
        atomizer,
        coordinator,
        archiver,
        shard,
        sentinel
    };

    /// Convert mock_system_module value to string.
    /// \param mod module to stringify.
    /// \return string representation of the mock system module value.
    auto mock_system_module_string(mock_system_module mod) -> std::string;

    /// Establishes dummy listeners for each enabled system module. For
    /// testing only.
    class mock_system {
      public:
        mock_system() = delete;

        /// Constructor.
        /// \param disabled_modules set of modules to disable so they can
        ///        be tested.
        /// \param opts pointer to configuration options to use.
        mock_system(
            const std::unordered_set<mock_system_module>& disabled_modules,
            config::options opts);

        ~mock_system();

        /// Launches servers for enabled modules. Raises googletest ASSERTs on
        /// failures. Following initialization, integration tests can connect
        /// to mock endpoints for each enabled module configured in the
        /// options set provided to the constructor.
        void init();

        /// Callers in integration tests can use this method to verify that
        /// modules under test transmit the messages they expect to
        /// counterpart modules.
        ///
        /// It registers an expected type of message to be received by a
        /// particular module (specifiable both by module type and by
        /// number—e.g., atomizer3). It then populates a future with the
        /// result, allowing callers to validate their expectations.
        ///
        /// \tparam T type of message expected.
        /// \param for_module kind of module expected to receive the message.
        /// \param reply_with optional message for the module to send back
        ///        over the network.
        /// \param module_id which specific component of kind `for_module`
        ///        should receive the message. (defaults to `0`)
        /// \return a future containing the deserialized message after the
        ///         module receives it.
        template<typename T>
        auto expect(mock_system_module for_module,
                    std::optional<cbdc::buffer>&& reply_with = std::nullopt,
                    uint64_t module_id = 0) -> std::future<T> {
            auto res_promise = std::make_shared<std::promise<T>>();
            auto res_future = res_promise->get_future();
            const std::lock_guard<std::mutex> guard(m_handler_lock);
            auto idx = std::make_pair(for_module, module_id);
            m_expect_handlers.try_emplace(
                idx,
                std::queue<cbdc::network::packet_handler_t>{});
            m_expect_handlers[idx].push(
                [&, prom = std::move(res_promise), rw = std::move(reply_with)](
                    cbdc::network::message_t&& pkt)
                    -> std::optional<cbdc::buffer> {
                    auto deser = cbdc::buffer_serializer(*pkt.m_pkt);
                    if constexpr(std::is_default_constructible_v<T>) {
                        T res;
                        deser >> res;
                        prom->set_value(res);
                    } else {
                        T res{deser};
                        prom->set_value(res);
                    }
                    return rw;
                });
            return res_future;
        }

        /// Broadcasts the provided data from one of the mock system network
        /// endpoints to connected peers. If the config defines multiple
        /// endpoints for a module, broadcasts from only the first one.
        /// \param origin module from which to broadcast the data.
        /// \param data data to broadcast.
        /// \return true if the data was successfully broadcast.
        template<typename Ta>
        [[nodiscard]] auto broadcast_from(mock_system_module origin,
                                          const Ta& data) -> bool {
            auto it = m_networks.find(origin);
            if((it == m_networks.end()) || it->second.empty()) {
                return false;
            }
            auto network = it->second[0];
            if(network->peer_count() == 0) {
                return false;
            }
            network->broadcast(data);
            return true;
        }

      private:
        std::mutex m_handler_lock;
        cbdc::config::options m_opts;
        std::unordered_map<mock_system_module,
                           std::vector<cbdc::network::endpoint_t>>
            m_module_endpoints;
        std::map<std::pair<mock_system_module, uint64_t>,
                 std::queue<cbdc::network::packet_handler_t>>
            m_expect_handlers;
        /// Mock networks, keyed by mock system module, ordered in vectors by
        /// their module ID specified in the provided configuration. Ex:
        ///     shard0_endpoint="127.0.0.1:8000"
        ///     shard1_endpoint="127.0.0.1:8001"
        /// => {mock_system_module::shard, {<cbdc::network::network 0>,
        /// <cbdc::network::network 1>}}
        std::unordered_map<
            mock_system_module,
            std::vector<std::shared_ptr<cbdc::network::connection_manager>>>
            m_networks;
        std::vector<std::thread> m_server_handlers;
        std::shared_ptr<cbdc::logging::log> m_logger{
            std::make_shared<cbdc::logging::log>(
                cbdc::logging::log_level::trace)};

        /// Starts servers for a mocked module.
        /// \param for_module module for which to start servers
        /// \param endpoints the network endpoints for the servers
        /// \return true if the servers are started successfully
        auto start_servers(mock_system_module for_module,
                           const std::vector<network::endpoint_t>& endpoints)
            -> bool;
    };
}

#endif // OPENCBDC_TX_TESTS_INTEGRATION_MOCK_SYSTEM_H_
