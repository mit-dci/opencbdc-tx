// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include "atomizer_raft.hpp"
#include "format.hpp"
#include "util/raft/serialization.hpp"
#include "util/raft/util.hpp"
#include "util/serialization/format.hpp"

#include <utility>

namespace cbdc::atomizer {
    controller::controller(uint32_t atomizer_id,
                           const config::options& opts,
                           std::shared_ptr<logging::log> log)
        : m_atomizer_id(atomizer_id),
          m_opts(opts),
          m_logger(std::move(log)),
          m_raft_node(
              static_cast<uint32_t>(atomizer_id),
              opts.m_atomizer_raft_endpoints[atomizer_id].value(),
              m_opts.m_stxo_cache_depth,
              m_logger,
              opts,
              [&](auto&& type, auto&& param) {
                  return raft_callback(std::forward<decltype(type)>(type),
                                       std::forward<decltype(param)>(param));
              },
              m_opts.m_wait_for_followers) {}

    controller::~controller() {
        m_raft_node.stop();
        m_atomizer_network.close();

        m_running = false;

        if(m_tx_notify_thread.joinable()) {
            m_tx_notify_thread.join();
        }

        if(m_atomizer_server.joinable()) {
            m_atomizer_server.join();
        }

        if(m_main_thread.joinable()) {
            m_main_thread.join();
        }

        m_notification_queue.clear();
        for(auto& t : m_notification_threads) {
            if(t.joinable()) {
                t.join();
            }
        }
    }

    auto controller::init() -> bool {
        if(!m_watchtower_network.cluster_connect(
               m_opts.m_watchtower_internal_endpoints)) {
            m_logger->error("Failed to connect to watchtowers.");
            return false;
        }

        auto raft_params = nuraft::raft_params();
        raft_params.election_timeout_lower_bound_
            = static_cast<int>(m_opts.m_election_timeout_lower);
        raft_params.election_timeout_upper_bound_
            = static_cast<int>(m_opts.m_election_timeout_upper);
        raft_params.heart_beat_interval_
            = static_cast<int>(m_opts.m_heartbeat);
        raft_params.snapshot_distance_
            = static_cast<int>(m_opts.m_snapshot_distance);
        raft_params.max_append_size_
            = static_cast<int>(m_opts.m_raft_max_batch);

        if(!m_raft_node.init(raft_params)) {
            return false;
        }

        auto raft_endpoints = std::vector<network::endpoint_t>();
        for(const auto& s : m_opts.m_atomizer_raft_endpoints) {
            raft_endpoints.push_back(*s);
        }
        if(!m_raft_node.build_cluster(raft_endpoints)) {
            return false;
        }

        m_tx_notify_thread = std::thread{[&] {
            tx_notify_handler();
        }};

        m_main_thread = std::thread{[&] {
            main_handler();
        }};

        auto n_threads = std::thread::hardware_concurrency();
        for(size_t i = 0; i < n_threads; i++) {
            m_notification_threads.emplace_back([&]() {
                notification_consumer();
            });
        }

        m_logger->info("Atomizer started...");

        return true;
    }

    auto controller::server_handler(cbdc::network::message_t&& pkt)
        -> std::optional<cbdc::buffer> {
        if(!m_raft_node.is_leader()) {
            return std::nullopt;
        }

        auto maybe_req = from_buffer<request>(*pkt.m_pkt);
        if(!maybe_req.has_value()) {
            m_logger->error("Invalid request packet");
            return std::nullopt;
        }

        std::visit(
            overloaded{
                [&](tx_notify_request& notif) {
                    m_logger->trace("Received transaction notification",
                                    to_string(notif.m_tx.m_id),
                                    "with height",
                                    notif.m_block_height);
                    m_notification_queue.push(notif);
                },
                [&](const prune_request& p) {
                    m_raft_node.make_request(p, nullptr);
                },
                [&](const get_block_request& g) {
                    auto result_fn = [&, peer_id = pkt.m_peer_id](
                                         raft::result_type& r,
                                         nuraft::ptr<std::exception>& err) {
                        if(err) {
                            m_logger->error("Exception handling log entry:",
                                            err->what());
                            return;
                        }

                        const auto res = r.get();
                        if(!res) {
                            m_logger->error("Requested block not found.");
                            return;
                        }

                        auto maybe_resp
                            = from_buffer<state_machine::response>(*res);
                        assert(maybe_resp.has_value());
                        assert(std::holds_alternative<get_block_response>(
                            maybe_resp.value()));
                        auto& resp
                            = std::get<get_block_response>(maybe_resp.value());
                        m_atomizer_network.send(resp.m_blk, peer_id);
                    };
                    m_raft_node.make_request(g, result_fn);
                }},
            maybe_req.value());

        return std::nullopt;
    }

    void controller::tx_notify_handler() {
        while(m_running) {
            if(!m_raft_node.send_complete_txs([&](auto&& res, auto&& err) {
                   err_return_handler(std::forward<decltype(res)>(res),
                                      std::forward<decltype(err)>(err));
               })) {
                static constexpr auto batch_send_delay
                    = std::chrono::milliseconds(20);
                std::this_thread::sleep_for(batch_send_delay);
            }
        }
    }

    void controller::main_handler() {
        auto last_time = std::chrono::high_resolution_clock::now();

        while(m_running) {
            const auto next_time
                = last_time
                + std::chrono::milliseconds(m_opts.m_target_block_interval);
            std::this_thread::sleep_until(next_time);
            last_time = std::chrono::high_resolution_clock::now();

            if(m_raft_node.is_leader()) {
                auto req = make_block_request();
                auto res
                    = m_raft_node.make_request(req, [&](auto&& r, auto&& err) {
                          raft_result_handler(
                              std::forward<decltype(r)>(r),
                              std::forward<decltype(err)>(err));
                      });
                if(!res && m_running) {
                    m_logger->error("Failed to make block at time",
                                    last_time.time_since_epoch().count());
                }
            }
        }
    }

    void controller::raft_result_handler(raft::result_type& r,
                                         nuraft::ptr<std::exception>& err) {
        if(err) {
            return;
        }

        const auto res = r.get();
        assert(res);
        auto maybe_resp = from_buffer<state_machine::response>(*res);
        assert(maybe_resp.has_value());
        assert(
            std::holds_alternative<make_block_response>(maybe_resp.value()));
        auto& resp = std::get<make_block_response>(maybe_resp.value());

        auto blk_pkt = make_shared_buffer(resp.m_blk);

        m_atomizer_network.broadcast(blk_pkt);

        m_logger->info("Block h:",
                       resp.m_blk.m_height,
                       ", nTXs:",
                       resp.m_blk.m_transactions.size(),
                       ", log idx:",
                       m_raft_node.last_log_idx(),
                       ", notifications:",
                       m_raft_node.tx_notify_count());

        if(!resp.m_errs.empty()) {
            auto buf = make_shared_buffer(resp.m_errs);
            m_watchtower_network.broadcast(buf);
        }
    }

    void controller::err_return_handler(raft::result_type& r,
                                        nuraft::ptr<std::exception>& err) {
        if(err) {
            std::cout << "Exception handling log entry: " << err->what()
                      << std::endl;
            return;
        }

        const auto res = r.get();
        if(res) {
            auto maybe_resp = from_buffer<state_machine::response>(*res);
            assert(maybe_resp.has_value());
            assert(std::holds_alternative<errors>(maybe_resp.value()));
            auto& resp = std::get<errors>(maybe_resp.value());
            m_watchtower_network.broadcast(resp);
        }
    }

    auto controller::raft_callback(nuraft::cb_func::Type type,
                                   nuraft::cb_func::Param* /* param */)
        -> nuraft::cb_func::ReturnCode {
        if(type == nuraft::cb_func::Type::BecomeFollower) {
            // We became a follower so shutdown the client network handler and
            // stop listening.
            m_atomizer_network.close();
            if(m_atomizer_server.joinable()) {
                m_atomizer_server.join();
            }
            m_logger->debug("Became follower, stopped listening");
        } else if(type == nuraft::cb_func::Type::BecomeLeader) {
            // We became the leader. Ensure the previous handler thread is
            // stopped and network shut down.
            m_atomizer_network.close();
            if(m_atomizer_server.joinable()) {
                m_atomizer_server.join();
            }
            // Reset the client network so we can use it again.
            m_atomizer_network.reset();
            // Start listening on our client endpoint and start the handler
            // thread.
            auto as = m_atomizer_network.start_server(
                m_opts.m_atomizer_endpoints[m_atomizer_id],
                [&](auto&& pkt) {
                    return server_handler(std::forward<decltype(pkt)>(pkt));
                });

            if(!as.has_value()) {
                m_logger->fatal("Failed to establish atomizer server.");
            }
            m_atomizer_server = std::move(as.value());
            m_logger->debug("Became leader, started listening");
        }
        return nuraft::cb_func::ReturnCode::Ok;
    }

    void controller::notification_consumer() {
        while(m_running) {
            auto notif = tx_notify_request();
            auto popped = m_notification_queue.pop(notif);
            if(!popped) {
                break;
            }
            m_raft_node.tx_notify(std::move(notif));
        }
    }
}
