// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include "format.hpp"
#include "uhs/transaction/messages.hpp"
#include "util/raft/serialization.hpp"
#include "util/rpc/tcp_server.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/util.hpp"

#include <utility>

namespace cbdc::coordinator {
    controller::controller(size_t node_id,
                           size_t coordinator_id,
                           config::options opts,
                           std::shared_ptr<logging::log> logger)
        : m_node_id(node_id),
          m_coordinator_id(coordinator_id),
          m_opts(std::move(opts)),
          m_logger(std::move(logger)),
          m_state_machine(nuraft::cs_new<state_machine>(m_logger)),
          m_shard_endpoints(m_opts.m_locking_shard_endpoints),
          m_shard_ranges(m_opts.m_shard_ranges),
          m_batch_size(m_opts.m_batch_size),
          m_exec_threads(m_opts.m_coordinator_max_threads) {
        m_raft_params.election_timeout_lower_bound_
            = static_cast<int>(m_opts.m_election_timeout_lower);
        m_raft_params.election_timeout_upper_bound_
            = static_cast<int>(m_opts.m_election_timeout_upper);
        m_raft_params.heart_beat_interval_
            = static_cast<int>(m_opts.m_heartbeat);
        m_raft_params.snapshot_distance_ = 0; // TODO: implement snapshots
        m_raft_params.max_append_size_
            = static_cast<int>(m_opts.m_raft_max_batch);
    }

    controller::~controller() {
        quit();
    }

    auto controller::init() -> bool {
        if(!m_logger) {
            std::cerr
                << "[ERROR] The logger pointer in coordinator::controller "
                << "is null." << std::endl;
            return false;
        }

        if(m_coordinator_id > (m_opts.m_coordinator_endpoints.size() - 1)) {
            m_logger->error("The coordinator ID is out of range "
                            "of the m_coordinator_endpoints vector.");
            return false;
        }

        for(const auto& vec : m_opts.m_coordinator_endpoints) {
            if(m_node_id > (vec.size() - 1)) {
                m_logger->error("The node ID is out of range "
                                "of the m_coordinator_endpoints vector.");
                return false;
            }
        }

        m_handler_endpoint
            = m_opts.m_coordinator_endpoints[m_coordinator_id][m_node_id];

        if(m_coordinator_id
           > (m_opts.m_coordinator_raft_endpoints.size() - 1)) {
            m_logger->error("The coordinator ID is out of range "
                            "of the m_coordinator_raft_endpoints vector.");
            return false;
        }

        for(const auto& vec : m_opts.m_coordinator_raft_endpoints) {
            if(m_node_id > (vec.size() - 1)) {
                m_logger->error("The node ID is out of range "
                                "of the m_coordinator_raft_endpoints vector.");
                return false;
            }
        }

        m_raft_serv = std::make_shared<raft::node>(
            static_cast<int>(m_node_id),
            m_opts.m_coordinator_raft_endpoints[m_coordinator_id],
            "coordinator" + std::to_string(m_coordinator_id),
            true,
            m_state_machine,
            0,
            m_logger,
            [&](auto&& res, auto&& err) {
                return raft_callback(std::forward<decltype(res)>(res),
                                     std::forward<decltype(err)>(err));
            });

        // Thread to handle starting and stopping the message handler and dtx
        // batch processing threads when triggered by the raft callback
        // becoming leader or follower
        m_start_thread = std::thread([&] {
            start_stop_func();
        });

        // Initialize NuRaft with the state machine we just created. Register
        // our callback function to notify us when we become a leader or
        // follower.
        return m_raft_serv->init(m_raft_params);
    }

    auto controller::raft_callback(nuraft::cb_func::Type type,
                                   nuraft::cb_func::Param* /* param */)
        -> nuraft::cb_func::ReturnCode {
        if(type == nuraft::cb_func::BecomeLeader) {
            // We're now the leader. Inform the start/stop thread that it
            // should start up the handler threads and initiate dtx recovery.
            // We do this via flags and a condition variable with the actual
            // start/stop in a separate thread to not block NuRaft internally.
            // Since we need to use the state machine to handle recovery we
            // need to return from this callback before we can start the
            // process.
            m_logger->warn("Became leader, starting coordinator");
            {
                std::unique_lock<std::mutex> l(m_start_mut);
                m_start_flag = true;
                m_stop_flag = false;
            }
            m_start_cv.notify_one();
            m_logger->warn("Done with become leader handler");
        } else if(type == nuraft::cb_func::BecomeFollower) {
            // As above, we're now a follower. Inform the start/stop thread to
            // kill the handler threads.
            m_logger->warn("Became follower, stopping coordinator");
            {
                std::unique_lock<std::mutex> l(m_start_mut);
                m_start_flag = false;
                m_stop_flag = true;
            }
            m_start_cv.notify_one();
            m_logger->warn("Done with become follower handler");
        }
        return nuraft::cb_func::ReturnCode::Ok;
    }

    auto
    controller::prepare_cb(const hash_t& dtx_id,
                           const std::vector<transaction::compact_tx>& txs)
        -> bool {
        // Send the prepare status for this dtx ID and the txs contained within
        // to the coordinator RSM and ensure it replicated (or failed) before
        // returning.
        auto comm = sm_command{{state_machine::command::prepare, dtx_id}, txs};
        return replicate_sm_command(comm).has_value();
    }

    auto
    controller::commit_cb(const hash_t& dtx_id,
                          const std::vector<bool>& complete_txs,
                          const std::vector<std::vector<uint64_t>>& tx_idxs)
        -> bool {
        // Send the commit status for this dtx ID and the result from prepare,
        // along with the mapping of which txs are relevant to each shard in
        // the prepare result to the RSM and check if it replicated.
        auto comm = sm_command{{state_machine::command::commit, dtx_id},
                               std::make_pair(complete_txs, tx_idxs)};
        return replicate_sm_command(comm).has_value();
    }

    auto controller::discard_cb(const hash_t& dtx_id) -> bool {
        // Send the discard status for this dtx ID and check if it replicated.
        auto comm = sm_command{{state_machine::command::discard, dtx_id}};
        return replicate_sm_command(comm).has_value();
    }

    auto controller::done_cb(const hash_t& dtx_id) -> bool {
        // Send the done status for this dtx ID and check if it replicated.
        auto comm = sm_command{{state_machine::command::done, dtx_id}};
        return replicate_sm_command(comm).has_value();
    }

    void controller::stop() {
        // Set the running flag to false, close the network and notify the
        // batch trigger condition variable so the threads that check them will
        // end and we can join the threads below.
        {
            std::lock_guard<std::mutex> l(m_batch_mut);
            m_running = false;
        }
        m_rpc_server.reset();
        m_batch_cv.notify_one();

        // Stop each of the locking shard clients to cancel any pending RPCs
        // and unblock any of the current dtxs so they can mark themselves as
        // failed and stop executing.
        {
            // The lock just protects m_shards which we're not modifying.
            std::shared_lock<std::shared_mutex> l(m_shards_mut);
            for(auto& s : m_shards) {
                s->stop();
            }
        }

        // Join the handler and batch execution threads
        if(m_batch_exec_thread.joinable()) {
            m_batch_exec_thread.join();
        }

        // Join any existing dtxs still executing
        join_execs();

        // Disconnect from the shards
        {
            std::unique_lock<std::shared_mutex> l(m_shards_mut);
            m_shards.clear();
        }
    }

    auto controller::recovery_func() -> bool {
        // Grab any non-completed dtxs from the state machine that were not
        // done when the previous leader failed.
        auto comm = sm_command{{state_machine::command::get}};
        m_logger->info("Waiting for get SM command response");
        auto r = replicate_sm_command(comm);
        if(!r) {
            // We likely stopped being the leader so we couldn't get the dtxs
            return false;
        }

        m_logger->info("Started recovery process");

        const auto& res = *r;
        if(!res) {
            // I don't think this should happen in practice. It might be wise
            // to make this fatal.
            m_logger->error("Empty response object");
            return false;
        }

        // List of coordinators/dtxs we're going to recover
        auto coordinators = std::vector<std::shared_ptr<distributed_tx>>();

        // Deserialize the coordinator state we just retrieved from the RSM
        auto state = coordinator_state();
        auto deser = nuraft_serializer(*res);
        deser >> state;

        for(const auto& prep : state.m_prepare_txs) {
            // Create a coordinator for the prepare dtx to recover
            auto coord = std::shared_ptr<distributed_tx>();
            {
                std::shared_lock<std::shared_mutex> l(m_shards_mut);
                coord = std::make_shared<distributed_tx>(prep.first,
                                                         m_shards,
                                                         m_logger);
            }
            // Tell the coordinator this dtx is in the prepare phase and
            // provide the list of transactions
            coord->recover_prepare(prep.second);
            coordinators.emplace_back(std::move(coord));
        }

        for(const auto& com : state.m_commit_txs) {
            auto coord = std::shared_ptr<distributed_tx>();
            {
                std::shared_lock<std::shared_mutex> l(m_shards_mut);
                coord = std::make_shared<distributed_tx>(com.first,
                                                         m_shards,
                                                         m_logger);
            }
            // Tell the coordinator this dtx is in the commit phase and provide
            // the flags for which dtxs to complete and the map between shards
            // and transactions in the batch
            coord->recover_commit(com.second.first, com.second.second);
            coordinators.emplace_back(std::move(coord));
        }

        for(const auto& dis : state.m_discard_txs) {
            auto coord = std::shared_ptr<distributed_tx>();
            {
                std::shared_lock<std::shared_mutex> l(m_shards_mut);
                coord = std::make_shared<distributed_tx>(dis,
                                                         m_shards,
                                                         m_logger);
            }
            // Tell the coordinator this dtx is in the discard phase
            coord->recover_discard();
            coordinators.emplace_back(std::move(coord));
        }

        // Flag in case one of the dtxs fails. This would happen if we stopped
        // being the leader mid-execution.
        auto success = std::atomic_bool{true};
        for(auto&& coord : coordinators) {
            // Register the callbacks for the RSM so we track dtx state during
            // execution
            batch_set_cbs(*coord);
            auto dtx_id_str = to_string(coord->get_id());
            m_logger->info("Recovering dtx", dtx_id_str);
            // Create a lambda that handles the execution and cleanup of the
            // dtx
            auto f = [&, c{std::move(coord)}, s{std::move(dtx_id_str)}](
                         size_t thread_idx) {
                // Execute the dtx from its most recent phase
                auto exec_res = c->execute();
                if(!exec_res) {
                    m_logger->error("Failed to recover dtx", s);
                    // We probably stopped being the leader so set the success
                    // flag
                    success = false;
                } else {
                    m_logger->info("Recovered dtx", s);
                }
                // Mark the thread we were using as done so it can be re-used
                {
                    std::shared_lock<std::shared_mutex> l(m_exec_mut);
                    m_exec_threads[thread_idx].second = false;
                }
            };
            // Schedule the lambda on an available executor thread. Blocks
            // until there's a thread available
            schedule_exec(std::move(f));
        }

        // Make sure we recovered fully before returning
        join_execs();

        return success;
    }

    void controller::batch_set_cbs(distributed_tx& c) {
        auto s = c.get_state();
        // Register all the RSM callbacks so the state machine tracks the state
        // of each outstanding dtxn. Don't register the callback for the phase
        // the dtxn is currently in as the state machine already knows about
        // it, we can skip re-notification.
        if(s != distributed_tx::dtx_state::prepare) {
            c.set_prepare_cb([&](auto&& dtx_id, auto&& txs) {
                return prepare_cb(std::forward<decltype(dtx_id)>(dtx_id),
                                  std::forward<decltype(txs)>(txs));
            });
        }
        if(s != distributed_tx::dtx_state::commit) {
            c.set_commit_cb(
                [&](auto&& dtx_id, auto&& complete_txs, auto&& tx_idxs) {
                    return commit_cb(
                        std::forward<decltype(dtx_id)>(dtx_id),
                        std::forward<decltype(complete_txs)>(complete_txs),
                        std::forward<decltype(tx_idxs)>(tx_idxs));
                });
        }
        if(s != distributed_tx::dtx_state::discard) {
            c.set_discard_cb([&](auto&& dtx_id) {
                return discard_cb(std::forward<decltype(dtx_id)>(dtx_id));
            });
        }
        if(s != distributed_tx::dtx_state::done) {
            c.set_done_cb([&](auto&& dtx_id) {
                return done_cb(std::forward<decltype(dtx_id)>(dtx_id));
            });
        }
    }

    void controller::batch_executor_func() {
        while(m_running) {
            {
                // Wait until there are transactions ready to be processed in a
                // dtx batch
                std::unique_lock<std::mutex> l(m_batch_mut);
                m_batch_cv.wait(l, [&]() {
                    return !m_current_txs->empty() || !m_running;
                });
            }
            if(!m_running) {
                break;
            }

            // Placeholders where we're going to move the current batch and map
            // of tx to sentinel so we can send the responses.
            auto batch = std::shared_ptr<distributed_tx>();
            auto txs
                = std::shared_ptr<decltype(m_current_txs)::element_type>();

            // New batch we're going to swap out with the current batch being
            // built by the handler thread
            auto new_batch = std::shared_ptr<distributed_tx>();
            {
                std::shared_lock<std::shared_mutex> l(m_shards_mut);
                new_batch
                    = std::make_shared<distributed_tx>(m_rnd.random_hash(),
                                                       m_shards,
                                                       m_logger);
            }

            // Atomically swap the current batch and tx->sentinel map with new
            // ones so we can run this batch while the handler thread builds a
            // new one
            {
                std::lock_guard<std::mutex> l(m_batch_mut);
                batch = std::move(m_current_batch);
                txs = std::move(m_current_txs);
                m_current_batch = std::move(new_batch);
                batch_set_cbs(*m_current_batch);
                m_current_txs = std::make_shared<
                    decltype(m_current_txs)::element_type>();
            }

            // Notify the handler thread it can re-start adding transactions to
            // the current batch
            m_batch_cv.notify_one();

            // Lambda to execute the batch and respond to the sentinel with the
            // result
            auto f = [&, b{std::move(batch)}, t{std::move(txs)}](
                         size_t thread_idx) {
                auto dtxid = to_string(b->get_id());
                m_logger->info("dtxn start:", dtxid, "size:", t->size());
                auto s = std::chrono::high_resolution_clock::now();
                // Execute the batch from the start
                auto res = b->execute();
                // For each tx result in the batch create a message with
                // the txid and the result, and send it to the appropriate
                // sentinel.
                for(const auto& [tx_id, metadata] : *t) {
                    const auto& [cb_func, batch_idx] = metadata;
                    auto tx_res = std::optional<bool>();
                    if(res.has_value()) {
                        tx_res = static_cast<bool>((*res)[batch_idx]);
                    }
                    cb_func(tx_res);
                }
                if(!res) {
                    // We probably stopped being the leader and we don't know
                    // the result of the txs so we can't respond to the
                    // sentinels. Just warn and clean up. The new leader will
                    // recover the dtx.
                    m_logger->warn("dtxn failed:", dtxid);
                } else {
                    auto e = std::chrono::high_resolution_clock::now();
                    auto l = (e - s).count();
                    m_logger->info("dtxn done:",
                                   dtxid,
                                   "t:",
                                   l,
                                   "size:",
                                   res->size());
                }
                // Mark our thread as done so it can be re-used
                {
                    std::shared_lock<std::shared_mutex> l(m_exec_mut);
                    m_exec_threads[thread_idx].second = false;
                }
            };
            // Schedule our executor lambda, block until there's a thread
            // available
            schedule_exec(std::move(f));
        }
    }

    auto controller::replicate_sm_command(const sm_command& c)
        -> std::optional<nuraft::ptr<nuraft::buffer>> {
        auto buf = nuraft::buffer::alloc(serialized_size(c));
        auto ser = nuraft_serializer(*buf);
        ser << c;
        // Sanity check to ensure total_sz was correct
        assert(ser.end_of_buffer());
        // Use synchronous mode to block until replication or failure
        return m_raft_serv->replicate_sync(buf);
    }

    void controller::connect_shards() {
        // Make a network::network for each shard cluster and a locking
        // shard client to manage RPCs. Add the clients to the m_shards map so
        // the dtxs can use them.
        for(size_t i{0}; i < m_shard_endpoints.size(); i++) {
            m_logger->warn("Connecting to shard cluster", std::to_string(i));
            auto s = std::make_shared<locking_shard::rpc::client>(
                m_shard_endpoints[i],
                m_shard_ranges[i],
                *m_logger);
            if(!s->init()) {
                m_logger->fatal("Failed to initialize shard client");
            }
            {
                std::unique_lock<std::shared_mutex> l(m_shards_mut);
                m_shards.emplace_back(std::move(s));
            }
        }
    }

    void controller::schedule_exec(std::function<void(size_t)>&& f) {
        // Loop until we successfully scheduled the given lambda on a thread
        bool found_thread{false};
        while(!found_thread) {
            {
                std::unique_lock<std::shared_mutex> l(m_exec_mut);
                for(size_t i{0}; i < m_exec_threads.size(); i++) {
                    auto& thr = m_exec_threads[i];
                    // If the thread is marked done we can use it
                    if(!thr.second) {
                        // Make sure the previous thread is joined
                        if(thr.first && thr.first->joinable()) {
                            thr.first->join();
                        }
                        // Mark the thread as in-use
                        thr.second = true;
                        // Start the thread with the given lambda and provide
                        // index of the thread so it can mark itself as done
                        // Not use-after-move because the outer loop exits once
                        // f has been moved due to found_thread.
                        thr.first // NOLINTNEXTLINE(bugprone-use-after-move)
                            = std::make_shared<std::thread>(std::move(f), i);
                        found_thread = true;
                        break;
                    }
                }
            }
            if(!found_thread) {
                // For now just yield to the scheduler if there were no
                // complete threads this time. In the future this could be a
                // condition variable instead.
                std::this_thread::yield();
            }
        }
    }

    void controller::join_execs() {
        std::shared_lock<std::shared_mutex> l(m_exec_mut);
        for(auto& t : m_exec_threads) {
            if(t.first && t.first->joinable()) {
                t.first->join();
            }
        }
    }

    void controller::start_stop_func() {
        while(!m_quit) {
            bool stopping{false};
            bool quitting{false};
            {
                // Wait until we're stopping, starting or quitting
                std::unique_lock<std::mutex> l(m_start_mut);
                m_start_cv.wait(l, [&]() {
                    return m_start_flag || m_quit || m_stop_flag;
                });
                // Store our plan of action so we can release the lock on these
                // flags in case the NuRaft handler needs to set them
                // differently while we're busy starting/stopping.
                if(m_quit) {
                    quitting = true;
                    stopping = true;
                } else {
                    // Sanity check: we should be stopping or starting, not
                    // both
                    assert(m_start_flag ^ m_stop_flag);
                    if(m_stop_flag) {
                        stopping = true;
                    }
                    m_start_flag = false;
                    m_stop_flag = false;
                }
            }

            if(stopping) {
                m_logger->warn("Stopping coordinator");
                stop();
                m_logger->warn("Stopped coordinator");
                if(quitting) {
                    m_logger->warn("Quitting");
                    break;
                }
            } else {
                m_logger->warn("Stopping coordinator before start");
                // Make sure the coordinator is stopped before starting it
                // again to satisfy any preconditions and not leave the
                // coordinator in a partial state
                stop();
                m_logger->warn("Starting coordinator");
                start();
                m_logger->warn("Started coordinator");
            }
        }
    }

    void controller::start() {
        // Set the running flag to true so when we start the threads they won't
        // immediately exit
        {
            std::lock_guard<std::mutex> l(m_batch_mut);
            m_running = true;
        }
        m_logger->warn("Resetting sentinel network handler");
        // Reset the handler network instance so we can re-use it
        m_rpc_server.reset();
        m_logger->warn("Connecting to shards");
        // Connect to the shard clusters
        connect_shards();
        m_logger->warn("Became leader, recovering dtxs");
        // Attempt recovery of existing dtxs until we stop being the leader or
        // recovery succeeds
        bool recovered{false};
        do {
            auto res = recovery_func();
            if(!res) {
                m_logger->error("Failed to recover, likely stopped "
                                "being leader");
                continue;
            }
            recovered = true;
        } while(!recovered && m_raft_serv->is_leader());
        m_logger->info("Recovery complete");

        // If we stopped being the leader while attempting to recover we
        // shouldn't bother starting and handler threads
        if(!m_raft_serv->is_leader()) {
            return;
        }

        // Create a fresh batch to add transactions to
        auto batch = std::shared_ptr<distributed_tx>();
        {
            std::shared_lock<std::shared_mutex> ll(m_shards_mut);
            batch = std::make_shared<distributed_tx>(m_rnd.random_hash(),
                                                     m_shards,
                                                     m_logger);
        }
        // Register the RSM callbacks with the batch
        batch_set_cbs(*batch);

        // Atomically set the current batch and a new tx->sentinel map
        {
            std::lock_guard<std::mutex> ll(m_batch_mut);
            m_current_batch = std::move(batch);
            m_current_txs
                = std::make_shared<decltype(m_current_txs)::element_type>();
        }

        // Start the batch executor thread
        m_batch_exec_thread = std::thread([&] {
            batch_executor_func();
        });

        // Listen on the coordinator endpoint and start handling incoming txs
        auto rpc_server = std::make_unique<cbdc::rpc::tcp_server<
            cbdc::rpc::async_server<rpc::request, rpc::response>>>(
            m_handler_endpoint);
        if(!rpc_server->init()) {
            m_logger->fatal("Failed to start RPC server");
        }

        m_rpc_server = std::make_unique<decltype(m_rpc_server)::element_type>(
            this,
            std::move(rpc_server));
    }

    void controller::quit() {
        // Notify the start/stop thread that we're quitting. One thread handles
        // starting and stopping to ensure only one thing is happening at a
        // time avoiding races on handler threads.
        {
            std::unique_lock<std::mutex> l(m_start_mut);
            m_quit = true;
        }
        m_start_cv.notify_one();
        if(m_start_thread.joinable()) {
            m_start_thread.join();
        }
    }

    auto controller::sm_command_header::operator==(
        const sm_command_header& rhs) const -> bool {
        return std::tie(m_comm, m_dtx_id)
            == std::tie(rhs.m_comm, rhs.m_dtx_id);
    }

    auto controller::coordinator_state::operator==(
        const coordinator_state& rhs) const -> bool {
        return std::tie(m_prepare_txs, m_commit_txs, m_discard_txs)
            == std::tie(rhs.m_prepare_txs,
                        rhs.m_commit_txs,
                        rhs.m_discard_txs);
    }

    auto controller::execute_transaction(transaction::compact_tx tx,
                                         callback_type result_callback)
        -> bool {
        // If we're not the leader we can't process txs
        if(!m_raft_serv->is_leader()) {
            return false;
        }

        if(!transaction::validation::check_attestations(
               tx,
               m_opts.m_sentinel_public_keys,
               m_opts.m_attestation_threshold)) {
            m_logger->warn("Received invalid compact transaction",
                           to_string(tx.m_id));
            return false;
        }

        auto added = [&]() {
            // Wait until there's space in the current batch
            std::unique_lock<std::mutex> l(m_batch_mut);
            m_batch_cv.wait(l, [&]() {
                return m_current_txs->size() < m_batch_size || !m_running;
            });
            if(!m_running) {
                return false;
            }

            // Make sure the TX is not already in the current batch
            if(m_current_txs->find(tx.m_id) != m_current_txs->end()) {
                return false;
            }
            // Add the tx to the current dtx batch and record its index
            auto idx = m_current_batch->add_tx(tx);
            // Map the index of the tx to the transaction ID and sentinel
            // ID
            m_current_txs->emplace(
                tx.m_id,
                std::make_pair(std::move(result_callback), idx));
            return true;
        }();
        if(added) {
            // If this was a new TX, notify the executor thread there's work to
            // do
            m_batch_cv.notify_one();
        }

        return added;
    }
}
