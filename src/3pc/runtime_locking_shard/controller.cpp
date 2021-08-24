// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include "format.hpp"
#include "state_machine.hpp"
#include "util/rpc/format.hpp"
#include "util/rpc/tcp_server.hpp"
#include "util/serialization/format.hpp"

namespace cbdc::threepc::runtime_locking_shard {
    controller::controller(size_t component_id,
                           size_t node_id,
                           network::endpoint_t server_endpoint,
                           std::vector<network::endpoint_t> raft_endpoints,
                           std::shared_ptr<logging::log> logger,
                           std::shared_ptr<cbdc::telemetry> tel)
        : m_logger(std::move(logger)),
          m_tel(std::move(tel)),
          m_state_machine(nuraft::cs_new<state_machine>()),
          m_raft_serv(std::make_shared<raft::node>(
              static_cast<int>(node_id),
              raft_endpoints,
              "runtime_locking_shard" + std::to_string(component_id),
              false,
              m_state_machine,
              0,
              m_logger,
              [&](auto&& res, auto&& err) {
                  return raft_callback(std::forward<decltype(res)>(res),
                                       std::forward<decltype(err)>(err));
              })),
          m_raft_client(
              std::make_shared<replicated_shard_client>(m_raft_serv)),
          m_raft_endpoints(std::move(raft_endpoints)),
          m_server_endpoint(std::move(server_endpoint)) {}

    auto controller::init() -> bool {
        auto params = nuraft::raft_params();
        params.snapshot_distance_ = 0; // TODO: implement snapshots
        params.max_append_size_ = config::defaults::raft_max_batch;
        params.election_timeout_lower_bound_
            = config::defaults::election_timeout_lower_bound;
        params.election_timeout_upper_bound_
            = config::defaults::election_timeout_upper_bound;
        params.heart_beat_interval_ = config::defaults::heartbeat;

        if(!m_raft_serv->init(params)) {
            m_logger->error("Failed to initialize raft server");
            return false;
        }

        return true;
    }

    auto controller::raft_callback(nuraft::cb_func::Type type,
                                   nuraft::cb_func::Param* /* param */)
        -> nuraft::cb_func::ReturnCode {
        if(type == nuraft::cb_func::Type::BecomeFollower) {
            m_logger->warn("Became follower, stopping listener");
            m_server.reset();
            return nuraft::cb_func::ReturnCode::Ok;
        }
        if(type == nuraft::cb_func::Type::BecomeLeader) {
            m_logger->warn("Became leader, starting listener");
            // Recover shard state from raft state machine
            do_recovery();
        }
        return nuraft::cb_func::ReturnCode::Ok;
    }

    void controller::do_recovery() {
        // Request tickets from state machine
        auto success = m_raft_client->get_tickets(
            [&](replicated_shard_interface::get_tickets_return_type res) {
                handle_get_tickets(std::move(res));
            });
        if(!success) {
            m_logger->error("Failed to request tickets from state machine");
        }
    }

    void controller::handle_get_tickets(
        replicated_shard_interface::get_tickets_return_type res) {
        if(!std::holds_alternative<replicated_shard_interface::tickets_type>(
               res)) {
            m_logger->error("Error requesting tickets from state machine");
            return;
        }

        m_shard = std::make_unique<impl>(m_logger, m_tel);

        auto&& tickets
            = std::get<replicated_shard_interface::tickets_type>(res);
        auto state = m_state_machine->get_shard()->get_state();
        auto success = m_shard->recover(state, tickets);
        if(!success) {
            m_logger->error("Error during shard recovery");
            return;
        }

        auto rpc_server = std::make_unique<cbdc::rpc::tcp_server<
            cbdc::rpc::async_server<rpc::request, rpc::response>>>(
            m_server_endpoint);
        if(!rpc_server->init()) {
            m_logger->fatal("Failed to start RPC server");
        }

        m_server = std::make_unique<decltype(m_server)::element_type>(
            m_logger,
            m_shard,
            m_raft_client,
            std::move(rpc_server));

        m_logger->info("Recovered shard and started RPC server");
    }
}
