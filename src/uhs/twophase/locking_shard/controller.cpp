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

    controller::~controller() {
        m_running = false;
        if(m_audit_thread.joinable()) {
            m_audit_thread.join();
        }
    }

    auto controller::init() -> bool {
        if(!m_logger) {
            std::cerr
                << "[ERROR] The logger pointer in locking_shard::controller"
                << " is null." << std::endl;
            return false;
        }

        m_audit_log.open(m_opts.m_shard_audit_logs[m_shard_id],
                         std::ios::app | std::ios::out);
        if(!m_audit_log.good()) {
            m_logger->error("Failed to open audit log");
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

        m_audit_thread = std::thread([this]() {
            audit();
        });

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

    void controller::audit() {
        while(m_running) {
            constexpr auto audit_wait_interval = std::chrono::seconds(1);
            std::this_thread::sleep_for(audit_wait_interval);
            if(!m_running) {
                break;
            }
            auto highest_epoch = m_shard->highest_epoch();
            if(highest_epoch - m_last_audit_epoch
               > m_opts.m_shard_audit_interval) {
                auto audit_epoch = highest_epoch;
                if(m_opts.m_shard_audit_interval > 0) {
                    audit_epoch
                        = (highest_epoch - m_opts.m_shard_audit_interval)
                        - (highest_epoch % m_opts.m_shard_audit_interval);
                }
                if(audit_epoch > highest_epoch
                   || audit_epoch <= m_last_audit_epoch) {
                    continue;
                }

                m_logger->info("Running Audit for", audit_epoch);
                auto maybe_commit = m_shard->get_summary(audit_epoch);
                if(!maybe_commit.has_value()) {
                    m_logger->error("Error running audit at epoch",
                                    audit_epoch);
                } else {
                    m_audit_log << audit_epoch << " " << to_string(maybe_commit.value())
                                << std::endl;
                    m_logger->info("Audit completed for", audit_epoch);

                    m_last_audit_epoch = audit_epoch;

                    m_shard->prune(audit_epoch);
                }
            }
        }
    }
}
