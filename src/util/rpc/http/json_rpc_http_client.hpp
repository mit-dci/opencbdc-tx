// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_JSON_RPC_HTTP_CLIENT_H_
#define OPENCBDC_TX_SRC_RPC_JSON_RPC_HTTP_CLIENT_H_

#include "event_handler.hpp"
#include "util/common/logging.hpp"

#include <curl/curl.h>
#include <functional>
#include <json/json.h>
#include <optional>
#include <queue>
#include <set>
#include <unordered_map>

namespace cbdc::rpc {
    /// Class for performing libcurl global initialization.
    class curl_initializer {
      public:
        /// Initializes libcurl.
        curl_initializer();
        /// Deinitializes libcurl.
        ~curl_initializer();
    };

    /// Singleton initializer to ensure libcurl is only initialized once per
    /// application.
    static curl_initializer curl_init = curl_initializer();

    /// Asynchronous HTTP JSON-RPC client implemented using libcurl. Supports
    /// randomized load balancing across multiple RPC endpoints.
    class json_rpc_http_client {
      public:
        /// Construct a new client.
        /// \param endpoints list of RPC endpoints to load balance between.
        /// \param timeout response timeout in milliseconds. 0 for no timeout.
        /// \param log log instance.
        json_rpc_http_client(std::vector<std::string> endpoints,
                             long timeout,
                             std::shared_ptr<logging::log> log);
        /// Cancels any existing requests and stops the client.
        ~json_rpc_http_client();

        json_rpc_http_client(const json_rpc_http_client&) = delete;
        auto operator=(const json_rpc_http_client&)
            -> json_rpc_http_client& = delete;
        json_rpc_http_client(json_rpc_http_client&&) = delete;
        auto
        operator=(json_rpc_http_client&&) -> json_rpc_http_client& = delete;

        /// Type alias for the response callback function.
        using callback_type = std::function<void(std::optional<Json::Value>)>;

        /// Calls the requested JSON-RPC method with the given parameters and
        /// returns the response asynchronously via a callback function.
        /// \param method method to call.
        /// \param params call parameters.
        /// \param result_fn function to call with response.
        void call(const std::string& method,
                  Json::Value params,
                  callback_type result_fn);

        /// Process events raised by the underlying libcurl implementation.
        /// \return true if the last pump was successful and should be
        ///         continued.
        [[nodiscard]] auto pump() -> bool;

      private:
        std::vector<std::string> m_endpoints;
        long m_timeout;
        std::unique_ptr<event_handler> m_ev_handler;

        Json::StreamWriterBuilder m_builder;

        CURLM* m_multi_handle{};

        std::queue<CURL*> m_handles;

        struct transfer {
            std::stringstream m_result;
            callback_type m_cb;
            std::string m_payload;
        };

        std::unordered_map<CURL*, std::unique_ptr<transfer>> m_transfers;

        curl_slist* m_headers{};
        Json::Value m_payload;

        size_t m_lb_idx{};

        // size_t m_requests_started{};

        static auto write_data(void* ptr,
                               size_t size,
                               size_t nmemb,
                               struct transfer* t) -> size_t;

        static auto socket_callback(CURL* handle,
                                    curl_socket_t s,
                                    int what,
                                    json_rpc_http_client* c,
                                    void* socketp) -> int;

        static auto timer_callback(CURLM* multi_handle,
                                   long timeout_ms,
                                   json_rpc_http_client* c) -> int;

      protected:
        std::shared_ptr<cbdc::logging::log> m_log;
    };
}

#endif
