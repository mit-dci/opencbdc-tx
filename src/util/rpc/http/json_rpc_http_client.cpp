// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "json_rpc_http_client.hpp"

#ifdef __APPLE__
#include "kqueue_event_handler.hpp"
#endif

#ifdef __linux__
#include "epoll_event_handler.hpp"
#endif

#include <cassert>

namespace cbdc::rpc {
    curl_initializer::curl_initializer() {
        curl_global_init(CURL_GLOBAL_ALL);
    }

    curl_initializer::~curl_initializer() {
        curl_global_cleanup();
    }

    json_rpc_http_client::json_rpc_http_client(
        std::vector<std::string> endpoints,
        long timeout,
        std::shared_ptr<logging::log> log)
        : m_endpoints(std::move(endpoints)),
          m_timeout(timeout),
          m_log(std::move(log)) {
// TODO: find a way to do this without the preprocessor
#ifdef __APPLE__
        m_ev_handler = std::make_unique<kqueue_event_handler>();
#endif
#ifdef __linux__
        m_ev_handler = std::make_unique<epoll_event_handler>();
#endif
        if(!m_ev_handler->init()) {
            return;
        }
        m_multi_handle = curl_multi_init();
        curl_multi_setopt(m_multi_handle,
                          CURLMOPT_TIMERFUNCTION,
                          timer_callback);
        curl_multi_setopt(m_multi_handle, CURLMOPT_TIMERDATA, this);
        curl_multi_setopt(m_multi_handle,
                          CURLMOPT_SOCKETFUNCTION,
                          socket_callback);
        curl_multi_setopt(m_multi_handle, CURLMOPT_SOCKETDATA, this);
        m_headers
            = curl_slist_append(m_headers, "Content-Type: application/json");
        m_headers = curl_slist_append(m_headers, "charsets: utf-8");
        m_payload["id"] = 1;
        m_payload["jsonrpc"] = "2.0";
    }

    json_rpc_http_client::~json_rpc_http_client() {
        for(auto& [handle, t] : m_transfers) {
            if(curl_multi_remove_handle(m_multi_handle, handle) != CURLM_OK) {
                m_log->fatal("Error removing handle");
            }
            curl_easy_cleanup(handle);
            t->m_cb(std::nullopt);
        }
        if(curl_multi_cleanup(m_multi_handle) != CURLM_OK) {
            m_log->fatal("Error cleaning up multi_handle");
        }
        while(!m_handles.empty()) {
            auto* handle = m_handles.front();
            curl_easy_cleanup(handle);
            m_handles.pop();
        }
        curl_slist_free_all(m_headers);
    }

    void json_rpc_http_client::call(const std::string& method,
                                    Json::Value params,
                                    callback_type result_fn) {
        CURL* handle{};
        if(m_handles.empty()) {
            handle = curl_easy_init();
            curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
            curl_easy_setopt(handle,
                             CURLOPT_URL,
                             m_endpoints[m_lb_idx].c_str());
            m_lb_idx = (m_lb_idx + 1) % m_endpoints.size();
            curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(handle, CURLOPT_HTTPHEADER, m_headers);
            curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, m_timeout);
            curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 3);
            // curl_easy_setopt(handle, CURLOPT_TIMEOUT, 5);
        } else {
            handle = m_handles.front();
            m_handles.pop();
        }

        auto it = m_transfers.emplace(handle, std::make_unique<transfer>());
        auto& tf = it.first->second;
        tf->m_cb = std::move(result_fn);

        curl_easy_setopt(handle, CURLOPT_WRITEDATA, tf.get());

        m_payload["method"] = method;
        m_payload["params"] = std::move(params);

        tf->m_payload = Json::writeString(m_builder, m_payload);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, tf->m_payload.c_str());

        if(curl_multi_add_handle(m_multi_handle, handle) != CURLM_OK) {
            m_log->fatal("Error adding handle");
        }

        // m_requests_started++;
        // m_log->trace("requests started:", m_requests_started);
    }

    auto json_rpc_http_client::write_data(void* ptr,
                                          size_t size,
                                          size_t nmemb,
                                          struct transfer* t) -> size_t {
        auto total_sz = size * nmemb;
        t->m_result.write(static_cast<char*>(ptr),
                          static_cast<std::streamsize>(total_sz));
        return total_sz;
    }

    auto json_rpc_http_client::pump() -> bool {
        auto maybe_events = m_ev_handler->poll();
        if(!maybe_events.has_value()) {
            m_log->error("Polling error");
            return false;
        }
        auto& events = maybe_events.value();
        if(events.empty()) {
            return true;
        }

        int running{};
        for(auto& [fd, is_timeout] : events) {
            if(is_timeout) {
                curl_multi_socket_action(m_multi_handle,
                                         CURL_SOCKET_TIMEOUT,
                                         0,
                                         &running);
                continue;
            }

            curl_multi_socket_action(m_multi_handle,
                                     static_cast<curl_socket_t>(fd),
                                     0,
                                     &running);
        }

        int q_depth{};
        do {
            auto* m = curl_multi_info_read(m_multi_handle, &q_depth);
            if(m == nullptr) {
                break;
            }

            auto it = m_transfers.extract(m->easy_handle);
            assert(!it.empty());
            auto& tf = it.mapped();

            if(m->msg != CURLMSG_DONE) {
                tf->m_cb(std::nullopt);
            } else {
                if(m->data.result != CURLE_OK) {
                    m_log->warn("CURL error:",
                                curl_easy_strerror(m->data.result));
                    auto* handle = m->easy_handle;
                    if(curl_multi_remove_handle(m_multi_handle, handle)
                       != CURLM_OK) {
                        m_log->error("Error removing multi handle");
                        return false;
                    }
                    if(curl_multi_add_handle(m_multi_handle, handle)
                       != CURLM_OK) {
                        m_log->error("Error adding multi handle");
                        return false;
                    }
                    m_transfers.insert(std::move(it));
                    continue;
                }

                long http_code = 0;
                curl_easy_getinfo(m->easy_handle,
                                  CURLINFO_RESPONSE_CODE,
                                  &http_code);

                if(http_code / 100 != 2) {
                    m_log->warn("Bad return code:", http_code);
                    tf->m_cb(std::nullopt);
                } else {
                    auto res = Json::Value();
                    auto r = Json::Reader();
                    auto success = r.parse(tf->m_result.str(), res, false);
                    if(success) {
                        // TODO: sanity check the value of res
                        tf->m_cb(std::move(res));
                    } else {
                        m_log->warn(r.getFormattedErrorMessages(),
                                    "res:",
                                    tf->m_result.str(),
                                    "(",
                                    tf->m_result.str().size(),
                                    ")");
                        tf->m_cb(std::nullopt);
                    }
                }
            }

            m_handles.push(m->easy_handle);
            if(curl_multi_remove_handle(m_multi_handle, m->easy_handle)
               != CURLM_OK) {
                m_log->error("Error removing multi handle");
                return false;
            }
        } while(q_depth > 0);

        return true;
    }

    auto json_rpc_http_client::socket_callback(CURL* /* handle */,
                                               curl_socket_t s,
                                               int what,
                                               json_rpc_http_client* c,
                                               void* /* socketp */) -> int {
        event_handler::event_type et{};
        switch(what) {
            case CURL_POLL_REMOVE:
                et = event_handler::event_type::remove;
                break;
            case CURL_POLL_INOUT:
                et = event_handler::event_type::inout;
                break;
            case CURL_POLL_IN:
                et = event_handler::event_type::in;
                break;
            case CURL_POLL_OUT:
                et = event_handler::event_type::out;
                break;
        }

        c->m_ev_handler->register_fd(s, et);

        return 0;
    }

    auto json_rpc_http_client::timer_callback(CURLM* /* multi_handle */,
                                              long timeout_ms,
                                              json_rpc_http_client* c) -> int {
        c->m_ev_handler->set_timeout(timeout_ms);
        return 0;
    }
}
