// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include "uhs/twophase/coordinator/format.hpp"
#include "util/rpc/tcp_server.hpp"
#include "util/serialization/util.hpp"
#include "util/serialization/util2.hpp"

#include <iostream>
#include <utility>

namespace cbdc::sentinel_2pc {
    controller::controller(uint32_t sentinel_id,
                           const config::options& opts,
                           std::shared_ptr<logging::log> logger)
        : m_sentinel_id(sentinel_id),
          m_opts(opts),
          m_logger(std::move(logger)),
          m_coordinator_client(
              opts.m_coordinator_endpoints[sentinel_id
                                           % static_cast<uint32_t>(
                                               opts.m_coordinator_endpoints
                                                   .size())]) {}

    auto controller::init() -> bool {
        if(!m_coordinator_client.init()) {
            m_logger->error("Failed to start coordinator client");
            return false;
        }

        auto rpc_server = std::make_unique<cbdc::rpc::tcp_server<
            cbdc::rpc::async_server<cbdc::sentinel::request,
                                    cbdc::sentinel::response>>>(
            m_opts.m_sentinel_endpoints[m_sentinel_id]);
        if(!rpc_server->init()) {
            m_logger->error("Failed to start sentinel RPC server");
            return false;
        }

        m_rpc_server = std::make_unique<decltype(m_rpc_server)::element_type>(
            this,
            std::move(rpc_server));

        return true;
    }

    auto controller::execute_transaction(transaction::full_tx tx,
                                         result_callback_type result_callback)
        -> bool {
        const auto validation_err = transaction::validation::check_tx(tx);
        if(validation_err.has_value()) {
            auto tx_id = transaction::tx_id(tx);
            m_logger->debug(
                "Rejected (",
                transaction::validation::to_string(validation_err.value()),
                ")",
                to_string(tx_id));
            result_callback(cbdc::sentinel::response{
                cbdc::sentinel::tx_status::static_invalid,
                validation_err});
            return true;
        }

        auto compact_tx = cbdc::transaction::compact_tx(tx);

        m_logger->debug("Accepted", to_string(compact_tx.m_id));

        auto cb =
            [&, res_cb = std::move(result_callback)](std::optional<bool> res) {
                result_handler(res, res_cb);
            };

        // TODO: add a "retry" error response to offload sentinels from this
        //       infinite retry responsibility.
        while(!m_coordinator_client.execute_transaction(compact_tx, cb)) {
            // TODO: the network currently doesn't provide a callback for
            //       reconnection events so we have to sleep here to
            //       prevent a needless spin. Instead, add such a callback
            //       or queue to the network to remove this sleep.
            static constexpr auto retry_delay = std::chrono::milliseconds(100);
            std::this_thread::sleep_for(retry_delay);
        };

        return true;
    }

    void controller::result_handler(std::optional<bool> res,
                                    const result_callback_type& res_cb) {
        if(res.has_value()) {
            auto resp = cbdc::sentinel::response{
                cbdc::sentinel::tx_status::confirmed,
                std::nullopt};
            if(!res.value()) {
                resp.m_tx_status = cbdc::sentinel::tx_status::state_invalid;
            }
            res_cb(resp);
        } else {
            res_cb(std::nullopt);
        }
    }
}
