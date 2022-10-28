// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "json_rpc_http_server.hpp"

#include <arpa/inet.h>
#include <iostream>
#include <thread>

namespace cbdc::rpc {
    json_rpc_http_server::json_rpc_http_server(network::endpoint_t endpoint,
                                               bool enable_cors)
        : m_host(endpoint.first),
          m_port(endpoint.second),
          m_enable_cors(enable_cors) {}

    json_rpc_http_server::~json_rpc_http_server() {
        // Set running flag
        m_running = false;

        // Stop accepting incoming connections
        auto sock = MHD_quiesce_daemon(m_daemon);

        // Wait for existing connections to drain
        for(;;) {
            const auto* inf
                = MHD_get_daemon_info(m_daemon,
                                      MHD_DAEMON_INFO_CURRENT_CONNECTIONS);
            if(inf->num_connections == 0) {
                break;
            }
            // std::cout << "Waiting for " << inf->num_connections << "
            // connections to close" << std::endl;
            constexpr auto wait_time = std::chrono::milliseconds(100);
            std::this_thread::sleep_for(wait_time);
        }

        // std::cout << "All connections closed" << std::endl;

        // Stop HTTP daemon
        MHD_stop_daemon(m_daemon);

        // Close listening socket
        if(sock != -1) {
            close(sock);
        }
    }

    auto json_rpc_http_server::init() -> bool {
        auto has_epoll
            = (MHD_is_feature_supported(MHD_FEATURE_EPOLL) == MHD_YES);
        auto use_flag
            = has_epoll ? MHD_USE_EPOLL_INTERNALLY : MHD_USE_POLL_INTERNALLY;
        auto connection_limit = has_epoll ? 65536 : FD_SETSIZE - 4;
        auto addr = sockaddr_in{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(m_port);
        inet_aton(m_host.c_str(),
                  reinterpret_cast<in_addr*>(&addr.sin_addr.s_addr));
        m_daemon = MHD_start_daemon(use_flag | MHD_ALLOW_SUSPEND_RESUME
                                        | MHD_USE_DEBUG,
                                    m_port,
                                    nullptr,
                                    nullptr,
                                    callback,
                                    this,
                                    MHD_OPTION_NOTIFY_COMPLETED,
                                    request_complete,
                                    this,
                                    MHD_OPTION_THREAD_POOL_SIZE,
                                    std::thread::hardware_concurrency(),
                                    MHD_OPTION_CONNECTION_TIMEOUT,
                                    3,
                                    MHD_OPTION_CONNECTION_LIMIT,
                                    connection_limit,
                                    MHD_OPTION_SOCK_ADDR,
                                    &addr,
                                    MHD_OPTION_END);
        return m_daemon != nullptr;
    }

    auto json_rpc_http_server::callback(void* cls,
                                        struct MHD_Connection* connection,
                                        const char* /* url */,
                                        const char* method,
                                        const char* /* version */,
                                        const char* upload_data,
                                        size_t* upload_data_size,
                                        void** con_cls) -> MHD_Result {
        if(*con_cls == nullptr) {
            auto new_req = std::make_unique<request>();
            new_req->m_connection = connection;
            auto* server = static_cast<json_rpc_http_server*>(cls);
            new_req->m_server = server;
            new_req->m_origin = MHD_lookup_connection_value(connection,
                                                            MHD_HEADER_KIND,
                                                            "Origin");
            *con_cls = new_req.get();
            {
                std::unique_lock l(server->m_requests_mut);
                server->m_requests.emplace(new_req.get(), std::move(new_req));
            }
            // server->m_requests_started++;
            return MHD_YES;
        }

        auto* req = static_cast<request*>(*con_cls);

        // cors response if enabled
        if(method == std::string("OPTIONS") && req->m_server->m_enable_cors) {
            send_cors_response(req);
            return MHD_YES;
        }

        if(method != std::string("POST")) {
            req->m_code = MHD_HTTP_METHOD_NOT_ALLOWED;
            send_response("HTTP method not allowed", req);
            return MHD_YES;
        }

        if(*upload_data_size != 0) {
            req->m_request.write(
                upload_data,
                static_cast<std::streamsize>(*upload_data_size));
            *upload_data_size = 0;
            return MHD_YES;
        }

        if(!req->m_server->m_running) {
            req->m_code = MHD_HTTP_SERVICE_UNAVAILABLE;
            send_response("Server is shutting down", req);
            return MHD_NO;
        }

        // std::cout << "Handling HTTP request with m_running " <<
        // req->m_server->m_running << std::endl;
        auto success = req->m_server->handle_request(req);
        if(!success) {
            req->m_code = MHD_HTTP_BAD_REQUEST;
            send_response("Invalid request payload", req);
        }

        return MHD_YES;
    }

    auto json_rpc_http_server::send_cors_response(request* request_info)
        -> bool {
        std::string response = "";
        auto* result = MHD_create_response_from_buffer(
            response.size(),
            static_cast<void*>(response.data()),
            MHD_RESPMEM_MUST_COPY);
        if(!request_info->m_origin) {
            request_info->m_origin = "*";
        }
        MHD_add_response_header(result,
                                "Access-Control-Allow-Origin",
                                request_info->m_origin);

        MHD_add_response_header(result,
                                "Access-Control-Allow-Methods",
                                "POST");
        MHD_add_response_header(result,
                                "Access-Control-Allow-Headers",
                                "Content-Type");
        MHD_add_response_header(result, "Access-Control-Max-Age", "600");
        MHD_add_response_header(result, "Vary", "Origin");
        MHD_add_response_header(result,
                                "Vary",
                                "Access-Control-Request-Method");
        MHD_add_response_header(result,
                                "Vary",
                                "Access-Control-Request-Headers");
        auto ret = MHD_queue_response(request_info->m_connection, 200, result);
        MHD_destroy_response(result);
        const auto* inf = MHD_get_connection_info(
            request_info->m_connection,
            MHD_CONNECTION_INFO_CONNECTION_SUSPENDED);
        if(inf->suspended == MHD_YES) {
            MHD_resume_connection(request_info->m_connection);
        }
        return ret == MHD_YES;
    }

    auto json_rpc_http_server::send_response(std::string response,
                                             request* request_info) -> bool {
        auto* result = MHD_create_response_from_buffer(
            response.size(),
            static_cast<void*>(response.data()),
            MHD_RESPMEM_MUST_COPY);

        if(request_info->m_server->m_enable_cors) {
            if(!request_info->m_origin) {
                request_info->m_origin = "*";
            }
            MHD_add_response_header(result,
                                    "Access-Control-Allow-Origin",
                                    request_info->m_origin);
            MHD_add_response_header(result, "Vary", "Origin");
        }

        MHD_add_response_header(result, "Content-Type", "application/json");
        auto ret = MHD_queue_response(request_info->m_connection,
                                      request_info->m_code,
                                      result);
        MHD_destroy_response(result);
        const auto* inf = MHD_get_connection_info(
            request_info->m_connection,
            MHD_CONNECTION_INFO_CONNECTION_SUSPENDED);
        if(inf->suspended == MHD_YES) {
            MHD_resume_connection(request_info->m_connection);
        }
        return ret == MHD_YES;
    }

    auto json_rpc_http_server::handle_request(request* request_info) -> bool {
        auto req = Json::Value();
        auto r = Json::Reader();
        auto success = r.parse(request_info->m_request.str(), req, false);
        if(!success) {
            return false;
        }

        if(!req.isMember("method")) {
            return false;
        }

        if(!req["method"].isString()) {
            return false;
        }

        auto method = req["method"].asString();
        auto params = Json::Value();

        if(req.isMember("params")) {
            params = req["params"];
        }

        uint64_t id = 0;
        if(req["id"].isUInt64()) {
            id = req["id"].asUInt64();
        }

        MHD_suspend_connection(request_info->m_connection);

        auto maybe_sent
            = m_cb(method,
                   params,
                   [this, request_info, id](std::optional<Json::Value> resp) {
                       handle_response(id, request_info, std::move(resp));
                   });
        return maybe_sent;
    }

    void
    json_rpc_http_server::handle_response(uint64_t id,
                                          request* request_info,
                                          std::optional<Json::Value> resp) {
        if(!resp.has_value()) {
            request_info->m_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
            request_info->m_server->send_response("Error processing request",
                                                  request_info);
            // std::cout << "request processing error" << std::endl;
            return;
        }

        auto resp_payload = resp.value();
        resp_payload["jsonrpc"] = "2.0";
        resp_payload["id"] = id;

        request_info->m_code = MHD_HTTP_OK;
        auto resp_str = Json::writeString(m_builder, resp_payload);
        request_info->m_server->send_response(resp_str, request_info);
    }

    void json_rpc_http_server::register_handler_callback(
        handler_callback_type handler_callback) {
        m_cb = std::move(handler_callback);
    }

    void json_rpc_http_server::request_complete(
        void* cls,
        struct MHD_Connection* /* connection */,
        void** con_cls,
        MHD_RequestTerminationCode /* toe */) {
        if(*con_cls == nullptr) {
            return;
        }
        auto* req = static_cast<request*>(*con_cls);
        auto* server = static_cast<json_rpc_http_server*>(cls);
        {
            std::unique_lock l(server->m_requests_mut);
            server->m_requests.erase(req);
            // std::cout << "waiting on requests " << server->m_requests.size()
            // << " " << server->m_requests_started << std::endl;
        }
    }
}
