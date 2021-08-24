// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "server.hpp"

#include "util/common/variant_overloaded.hpp"

namespace cbdc::threepc::runtime_locking_shard::rpc {
    server::server(
        std::shared_ptr<logging::log> logger,
        std::shared_ptr<interface> impl,
        std::shared_ptr<replicated_shard_interface> repl,
        std::unique_ptr<cbdc::rpc::async_server<request, response>> srv)
        : m_log(std::move(logger)),
          m_impl(std::move(impl)),
          m_repl(std::move(repl)),
          m_srv(std::move(srv)) {
        m_srv->register_handler_callback(
            [&](const request& req,
                std::function<void(std::optional<response>)> callback) {
                return handler_callback(req, std::move(callback));
            });
    }

    auto server::handler_callback(
        const request& req,
        std::function<void(std::optional<response>)> callback) -> bool {
        auto success = std::visit(
            overloaded{
                [&](const rpc::try_lock_request& msg) {
                    return m_impl->try_lock(
                        msg.m_ticket_number,
                        msg.m_broker_id,
                        msg.m_key,
                        msg.m_locktype,
                        msg.m_first_lock,
                        [callback](interface::try_lock_return_type ret) {
                            callback(std::move(ret));
                        });
                },
                [&](const rpc::prepare_request& msg) {
                    return m_impl->prepare(
                        msg.m_ticket_number,
                        msg.m_broker_id,
                        msg.m_state_updates,
                        [this, callback, msg](
                            interface::prepare_return_type ret) {
                            handle_prepare(std::move(ret), msg, callback);
                        });
                },
                [&](rpc::commit_request msg) {
                    return m_repl->commit(
                        msg.m_ticket_number,
                        [this, callback, msg](
                            replicated_shard_interface::return_type ret) {
                            handle_commit(ret, msg, callback);
                        });
                },
                [&](rpc::rollback_request msg) {
                    return m_repl->finish(
                        msg.m_ticket_number,
                        [this, callback, msg](
                            replicated_shard_interface::return_type ret) {
                            do_rollback(ret, msg, callback);
                        });
                },
                [&](rpc::finish_request msg) {
                    return m_repl->finish(
                        msg.m_ticket_number,
                        [this, callback, msg](
                            replicated_shard_interface::return_type ret) {
                            do_finish(ret, msg, callback);
                        });
                },
                [&](rpc::get_tickets_request msg) {
                    return m_impl->get_tickets(
                        msg.m_broker_id,
                        [callback](interface::get_tickets_return_type ret) {
                            callback(std::move(ret));
                        });
                }},
            req);
        return success;
    }

    void server::handle_prepare(interface::prepare_return_type ret,
                                const rpc::prepare_request& msg,
                                const callback_type& callback) {
        if(ret.has_value()) {
            m_log->trace("Error response during prepare");
            callback(std::move(ret));
            return;
        }

        auto success = m_repl->prepare(
            msg.m_ticket_number,
            msg.m_broker_id,
            msg.m_state_updates,
            [this, callback](replicated_shard_interface::return_type res) {
                if(res.has_value()) {
                    m_log->error("Error response during prepare replication");
                    callback(error_code::internal_error);
                    return;
                }
                callback(interface::prepare_return_type{});
            });
        if(!success) {
            m_log->error("Error replicating prepare");
            callback(error_code::internal_error);
        }
    }

    void server::handle_commit(replicated_shard_interface::return_type ret,
                               rpc::commit_request msg,
                               const callback_type& callback) {
        if(ret.has_value()) {
            m_log->error("Error response during commit replication");
            callback(error_code::internal_error);
            return;
        }

        auto success
            = m_impl->commit(msg.m_ticket_number,
                             [callback](interface::commit_return_type res) {
                                 callback(std::move(res));
                             });
        if(!success) {
            m_log->error("Error initiating commit with internal shard");
            callback(error_code::internal_error);
        }
    }

    void server::do_rollback(replicated_shard_interface::return_type ret,
                             rpc::rollback_request msg,
                             const callback_type& callback) {
        if(ret.has_value()) {
            m_log->error("Error response during discard replication",
                         static_cast<int>(ret.value()));
            callback(error_code::internal_error);
            return;
        }

        auto success = m_impl->rollback(
            msg.m_ticket_number,
            [callback](interface::rollback_return_type res) {
                callback(std::move(res));
            });
        if(!success) {
            m_log->error("Error initiating rollback with internal shard");
            callback(error_code::internal_error);
        }
    }

    void server::do_finish(replicated_shard_interface::return_type ret,
                           rpc::finish_request msg,
                           const callback_type& callback) {
        if(ret.has_value()) {
            m_log->error("Error response during discard replication");
            callback(error_code::internal_error);
            return;
        }

        auto success
            = m_impl->finish(msg.m_ticket_number,
                             [callback](interface::finish_return_type res) {
                                 callback(std::move(res));
                             });
        if(!success) {
            m_log->error("Error initiating rollback with internal shard");
            callback(error_code::internal_error);
        }
    }
}
