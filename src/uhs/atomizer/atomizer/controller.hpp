// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_ATOMIZER_CONTROLLER_H_
#define OPENCBDC_TX_SRC_ATOMIZER_CONTROLLER_H_

#include "atomizer_raft.hpp"
#include "uhs/atomizer/atomizer/block.hpp"
#include "util/common/config.hpp"
#include "util/network/connection_manager.hpp"

#include <memory>

namespace cbdc::atomizer {
    /// Wrapper for the atomizer raft executable implementation.
    class controller {
      public:
        controller() = delete;
        controller(const controller&) = delete;
        auto operator=(const controller&) -> controller& = delete;
        controller(controller&&) = delete;
        auto operator=(controller&&) -> controller& = delete;

        /// Constructor.
        /// \param atomizer_id the running ID of this atomizer.
        /// \param opts configuration options.
        /// \param log pointer to shared logger.
        controller(uint32_t atomizer_id,
                   const config::options& opts,
                   std::shared_ptr<logging::log> log);

        ~controller();

        /// Initializes the controller.
        /// \return true if initialization succeeded.
        auto init() -> bool;

      private:
        uint32_t m_atomizer_id;
        cbdc::config::options m_opts;
        std::shared_ptr<logging::log> m_logger;

        atomizer_raft m_raft_node;
        std::mutex m_pending_txnotify_mut;
        std::condition_variable m_pending_txnotify_cv;
        std::queue<cbdc::network::message_t> m_pending_txnotify{};
        std::atomic_bool m_running{true};

        cbdc::network::connection_manager m_watchtower_network;
        cbdc::network::connection_manager m_atomizer_network;

        std::thread m_atomizer_server;
        std::thread m_tx_notify_thread;
        std::thread m_main_thread;

        auto server_handler(cbdc::network::message_t&& pkt)
            -> std::optional<cbdc::buffer>;
        void tx_notify_handler();
        void main_handler();
        void raft_result_handler(raft::result_type& r,
                                 nuraft::ptr<std::exception>& err);
        void err_return_handler(raft::result_type& r,
                                nuraft::ptr<std::exception>& err);
        auto raft_callback(nuraft::cb_func::Type type,
                           nuraft::cb_func::Param* param)
            -> nuraft::cb_func::ReturnCode;
    };
}

#endif // OPENCBDC_TX_SRC_ATOMIZER_CONTROLLER_H_
