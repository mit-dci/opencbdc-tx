// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include "state_machine.hpp"
#include "util/rpc/format.hpp"
#include "util/rpc/tcp_server.hpp"
#include "util/serialization/format.hpp"

namespace cbdc::parsec::ticket_machine {
    controller::controller(size_t node_id,
                           network::endpoint_t server_endpoint,
                           std::vector<network::endpoint_t> raft_endpoints,
                           std::shared_ptr<logging::log> logger)
        : m_logger(std::move(logger)),
          m_state_machine(
              nuraft::cs_new<state_machine>(m_logger, m_batch_size)),
          m_raft_serv(std::make_shared<raft::node>(
              static_cast<int>(node_id),
              raft_endpoints,
              "ticket_machine",
              false,
              m_state_machine,
              0,
              m_logger,
              [&](auto&& res, auto&& err) {
                  return raft_callback(std::forward<decltype(res)>(res),
                                       std::forward<decltype(err)>(err));
              })),
          m_raft_endpoints(std::move(raft_endpoints)),
          m_server_endpoint(std::move(server_endpoint)) {}

    auto controller::init() -> bool {
        auto params = nuraft::raft_params();
        params.snapshot_distance_ = 0; // TODO: implement snapshots

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
            m_server = std::make_unique<decltype(m_server)::element_type>(
                m_server_endpoint);
            m_server->register_raft_node(m_raft_serv);
            if(!m_server->init()) {
                m_logger->fatal("Couldn't start message handler server");
            }
        }
        return nuraft::cb_func::ReturnCode::Ok;
    }
}
