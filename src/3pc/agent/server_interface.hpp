// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CBDC_UNIVERSE0_SRC_3PC_AGENT_SERVER_INTERFACE_H_
#define CBDC_UNIVERSE0_SRC_3PC_AGENT_SERVER_INTERFACE_H_

#include "3pc/agent/impl.hpp"
#include "3pc/broker/interface.hpp"
#include "3pc/directory/interface.hpp"
#include "interface.hpp"
#include "messages.hpp"
#include "util/common/blocking_queue.hpp"
#include "util/common/thread_pool.hpp"

#include <atomic>
#include <memory>
#include <secp256k1.h>
#include <thread>

namespace cbdc::threepc::agent::rpc {
    /// Server interface for an agent. Subclasses should implement specific
    /// handling logic depending on the runner type.
    class server_interface {
      public:
        /// Constructor. Registers the agent implementation with the
        /// RPC server using a request handler callback.
        /// \param broker broker instance.
        /// \param log log instance.
        /// \param cfg system configuration options.
        server_interface(std::shared_ptr<broker::interface> broker,
                         std::shared_ptr<logging::log> log,
                         const cbdc::threepc::config& cfg);

        /// Stops retrying additional transactions and cleans up the runners.
        virtual ~server_interface();

        /// Initializes the server, starts processing requests.
        virtual auto init() -> bool = 0;

        server_interface(const server_interface&) = delete;
        auto operator=(const server_interface&) -> server_interface& = delete;
        server_interface(server_interface&&) = delete;
        auto operator=(server_interface&&) -> server_interface& = delete;

      private:
        friend class server;
        friend class http_server;

        std::shared_ptr<broker::interface> m_broker;
        std::shared_ptr<logging::log> m_log;
        const cbdc::threepc::config& m_cfg;

        mutable std::mutex m_agents_mut;
        std::atomic<size_t> m_next_id{};
        std::unordered_map<size_t, std::shared_ptr<agent::impl>> m_agents;

        blocking_queue<size_t> m_cleanup_queue;
        std::thread m_cleanup_thread;

        blocking_priority_queue<size_t, std::greater<>> m_retry_queue;
        std::thread m_retry_thread;

        std::shared_ptr<thread_pool> m_threads{
            std::make_shared<thread_pool>()};

        std::shared_ptr<secp256k1_context> m_secp{
            secp256k1_context_create(SECP256K1_CONTEXT_SIGN
                                     | SECP256K1_CONTEXT_VERIFY),
            &secp256k1_context_destroy};
    };
}

#endif
