// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "server.hpp"

#include "3pc/agent/runners/lua/impl.hpp"
#include "agent/format.hpp"
#include "impl.hpp"
#include "util/rpc/format.hpp"
#include "util/serialization/format.hpp"

#include <cassert>
#include <future>

namespace cbdc::threepc::agent::rpc {
    server::server(std::unique_ptr<server_type> srv,
                   std::shared_ptr<broker::interface> broker,
                   std::shared_ptr<logging::log> log,
                   std::shared_ptr<cbdc::telemetry> tel,
                   const cbdc::threepc::config& cfg)
        : server_interface(std::move(broker),
                           std::move(log),
                           std::move(tel),
                           cfg),
          m_srv(std::move(srv)) {
        m_srv->register_handler_callback(
            [&](request req, server_type::response_callback_type callback) {
                return request_handler(std::move(req), std::move(callback));
            });
    }

    server::~server() {
        m_log->trace("Agent server shutting down...");
        m_srv.reset();
        m_log->trace("Shut down agent server");
    }

    auto server::init() -> bool {
        return m_srv->init();
    }

    auto server::request_handler(request req,
                                 server_type::response_callback_type callback)
        -> bool {
        m_log->trace("received request with m_function ",
                     req.m_function.to_hex(),
                     " and param size ",
                     req.m_param.size());
        auto id = m_next_id++;
        auto a = [&]() {
            auto agent = std::make_shared<impl>(
                m_log,
                m_cfg,
                &runner::factory<runner::lua_runner>::create,
                m_broker,
                req.m_function,
                req.m_param,
                [this, id, callback](interface::exec_return_type res) {
                    auto success = std::holds_alternative<return_type>(res);
                    if(!success) {
                        auto ec = std::get<interface::error_code>(res);
                        if(ec == interface::error_code::retry) {
                            m_retry_queue.push(id);
                            return;
                        }
                    }
                    callback(res);
                    m_cleanup_queue.push(id);
                },
                runner::lua_runner::initial_lock_type,
                req.m_dry_run,
                m_secp,
                m_threads,
                m_tel);
            {
                std::unique_lock l(m_agents_mut);
                m_agents.emplace(id, agent);
            }
            return agent;
        }();
        return a->exec();
    }
}
