// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_JSON_RPC_HTTP_SERVER_H_
#define OPENCBDC_TX_SRC_RPC_JSON_RPC_HTTP_SERVER_H_

#include "util/network/socket.hpp"

#include <atomic>
#include <functional>
#include <json/json.h>
#include <microhttpd.h>
#include <mutex>
#include <optional>
#include <sstream>

namespace cbdc::rpc {
    /// Asynchrounous HTTP JSON-RPC server implemented using libmicrohttpd and
    /// libjsoncpp.
    class json_rpc_http_server {
      public:
        /// Type alias for the callback function for returning response values
        /// to the server.
        using result_callback_type
            = std::function<void(std::optional<Json::Value>)>;
        /// Callback function type provided by the application for processing
        /// requests.
        using handler_callback_type = std::function<
            bool(std::string, Json::Value, result_callback_type)>;

        /// Construct a new server.
        /// \param endpoint network endpoint to listen on.
        /// \param enable_cors true if CORS should be enabled.
        explicit json_rpc_http_server(network::endpoint_t endpoint,
                                      bool enable_cors = false);

        /// Stop the server.
        ~json_rpc_http_server();

        json_rpc_http_server(const json_rpc_http_server&) = delete;
        auto operator=(const json_rpc_http_server&)
            -> json_rpc_http_server& = delete;
        json_rpc_http_server(json_rpc_http_server&&) = delete;
        auto
        operator=(json_rpc_http_server&&) -> json_rpc_http_server& = delete;

        /// Register the application request handler function with the server.
        void register_handler_callback(handler_callback_type handler_callback);

        /// Start listening for incoming connections and processing requests.
        /// \return true if listening was successful.
        auto init() -> bool;

      private:
        struct request {
            MHD_Connection* m_connection{};
            std::stringstream m_request;
            json_rpc_http_server* m_server{};
            const char* m_origin{};
            unsigned int m_code{};
        };

        network::ip_address m_host{};
        uint16_t m_port{};
        MHD_Daemon* m_daemon{};
        handler_callback_type m_cb;
        Json::StreamWriterBuilder m_builder;

        std::mutex m_requests_mut;
        std::map<request*, std::unique_ptr<request>> m_requests;

        std::atomic<bool> m_running{true};
        bool m_enable_cors;
        // std::atomic<size_t> m_requests_started{};

        static auto callback(void* cls,
                             struct MHD_Connection* connection,
                             const char* url,
                             const char* method,
                             const char* version,
                             const char* upload_data,
                             size_t* upload_data_size,
                             void** con_cls) -> MHD_Result;

        static auto send_cors_response(request* request_info) -> bool;
        static auto send_response(std::string response,
                                  request* request_info) -> bool;

        auto handle_request(request* request_info) -> bool;

        void handle_response(uint64_t id,
                             request* request_info,
                             std::optional<Json::Value> resp);

        static void request_complete(void* cls,
                                     struct MHD_Connection* connection,
                                     void** con_cls,
                                     MHD_RequestTerminationCode toe);
    };
}

#endif
