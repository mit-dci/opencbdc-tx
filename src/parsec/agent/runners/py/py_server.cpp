// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "py_server.hpp"

#include "agent/format.hpp"
#include "impl.hpp"
#include "parsec/agent/runners/py/impl.hpp"
#include "util/rpc/format.hpp"
#include "util/serialization/format.hpp"

#include <cassert>
#include <future>

namespace cbdc::parsec::agent::rpc {
    py_server::py_server(std::unique_ptr<server_type> srv,
                         std::shared_ptr<broker::interface> broker,
                         std::shared_ptr<logging::log> log,
                         const cbdc::parsec::config& cfg)
        : server_interface(std::move(broker), std::move(log), cfg),
          m_srv(std::move(srv)) {
        m_srv->register_handler_callback(
            [&](request req, server_type::response_callback_type callback) {
                return request_handler(std::move(req), std::move(callback));
            });
    }

    py_server::~py_server() {
        m_log->trace("Agent py_server shutting down...");
        m_srv.reset();
        m_log->trace("Shut down agent py_server");
    }

    auto py_server::init() -> bool {
        return m_srv->init();
    }

    auto
    py_server::request_handler(request req,
                               server_type::response_callback_type callback)
        -> bool {
        auto id = m_next_id++;
        auto a = [&]() {
            auto agent = std::make_shared<impl>(
                m_log,
                m_cfg,
                &runner::factory<runner::py_runner>::create,
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
                runner::py_runner::initial_lock_type,
                req.m_is_readonly_run,
                m_secp,
                m_threads);
            {
                std::unique_lock l(m_agents_mut);
                m_agents.emplace(id, agent);
            }
            return agent;
        }();
        return a->exec();
    }
}
