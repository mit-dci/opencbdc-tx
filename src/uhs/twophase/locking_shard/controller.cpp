// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include "format.hpp"
#include "state_machine.hpp"
#include "status_client.hpp"
#include "util/rpc/tcp_server.hpp"
#include "util/serialization/format.hpp"
#include "util/common/commitment.hpp"

#include <utility>

namespace cbdc::locking_shard {
    controller::controller(size_t shard_id,
                           size_t node_id,
                           config::options opts,
                           std::shared_ptr<logging::log> logger)
        : m_opts(std::move(opts)),
          m_logger(std::move(logger)),
          m_shard_id(shard_id),
          m_node_id(node_id),
          m_preseed_dir(
              m_opts.m_seed_from != m_opts.m_seed_to
                  ? "2pc_shard_preseed_"
                        + std::to_string(m_opts.m_seed_to - m_opts.m_seed_from)
                        + "_" + std::to_string(m_shard_id)
                  : "") {}

    auto controller::init() -> bool {
        if(!m_logger) {
            std::cerr
                << "[ERROR] The logger pointer in locking_shard::controller"
                << " is null." << std::endl;
            return false;
        }

        auto params = nuraft::raft_params();
        params.election_timeout_lower_bound_
            = static_cast<int>(m_opts.m_election_timeout_lower);
        params.election_timeout_upper_bound_
            = static_cast<int>(m_opts.m_election_timeout_upper);
        params.heart_beat_interval_ = static_cast<int>(m_opts.m_heartbeat);
        params.snapshot_distance_ = 0; // TODO: implement snapshots
        params.max_append_size_ = static_cast<int>(m_opts.m_raft_max_batch);

        if(m_shard_id > (m_opts.m_shard_ranges.size() - 1)) {
            m_logger->error(
                "The shard ID is out of range of the m_shard_ranges vector.");
            return false;
        }

        m_state_machine = nuraft::cs_new<state_machine>(
            m_opts.m_shard_ranges[m_shard_id],
            m_logger,
            m_opts.m_shard_completed_txs_cache_size,
            m_preseed_dir,
            m_opts);

        m_shard = m_state_machine->get_shard_instance();

        if(m_shard_id > (m_opts.m_locking_shard_raft_endpoints.size() - 1)) {
            m_logger->error("The shard ID is out of range "
                            "of the m_locking_shard_raft_endpoints vector.");
            return false;
        }

        for(const auto& vec : m_opts.m_locking_shard_raft_endpoints) {
            if(m_node_id > (vec.size() - 1)) {
                m_logger->error(
                    "The node ID is out of range "
                    "of the m_locking_shard_raft_endpoints vector.");
                return false;
            }
        }

        m_raft_serv = std::make_shared<raft::node>(
            static_cast<int>(m_node_id),
            m_opts.m_locking_shard_raft_endpoints[m_shard_id],
            "shard" + std::to_string(m_shard_id),
            false,
            m_state_machine,
            0,
            m_logger,
            [&](auto&& res, auto&& err) {
                return raft_callback(std::forward<decltype(res)>(res),
                                     std::forward<decltype(err)>(err));
            });

        if(!m_raft_serv->init(params)) {
            m_logger->error("Failed to initialize raft server");
            return false;
        }

        auto status_rpc_server = std::make_unique<
            cbdc::rpc::blocking_tcp_server<rpc::status_request,
                                           rpc::status_response>>(
            m_opts.m_locking_shard_readonly_endpoints[m_shard_id][m_node_id]);
        if(!status_rpc_server->init()) {
            m_logger->error("Failed to start status RPC server");
            return false;
        }

        m_status_server
            = std::make_unique<decltype(m_status_server)::element_type>(
                m_shard,
                std::move(status_rpc_server));

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
                m_opts.m_locking_shard_endpoints[m_shard_id][m_node_id]);
            m_server->register_raft_node(m_raft_serv);
            if(!m_server->init()) {
                m_logger->fatal("Couldn't start message handler server");
            }
        }
        return nuraft::cb_func::ReturnCode::Ok;
    }
}
