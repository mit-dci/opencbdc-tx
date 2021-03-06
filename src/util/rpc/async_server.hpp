// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_ASYNC_SERVER_H_
#define OPENCBDC_TX_SRC_RPC_ASYNC_SERVER_H_

#include "server.hpp"

namespace cbdc::rpc {
    /// \brief Generic asynchronous RPC server.
    /// Handles serialization of requests and responses. Dispatches incoming
    /// requests to a handler callback for processing and returns the response
    /// using a response callback. Subclass to define specific remote
    /// communication logic.
    /// \tparam Request type for requests.
    /// \tparam Response type for responses.
    /// \tparam InBuffer type of buffer for serialized requests, defaults to
    ///         \ref cbdc::buffer
    /// \tparam OutBuffer type of buffer for serialized responses, defaults to
    ///         \ref cbdc::buffer
    template<typename Request,
             typename Response,
             typename InBuffer = buffer,
             typename OutBuffer = buffer>
    class async_server
        : public server<Request, Response, InBuffer, OutBuffer> {
      public:
        async_server() = default;
        async_server(async_server&&) noexcept = default;
        auto operator=(async_server&&) noexcept -> async_server& = default;
        async_server(const async_server&) = default;
        auto operator=(const async_server&) -> async_server& = default;

        ~async_server() override = default;

        static constexpr handler_type handler = handler_type::async;

        /// \brief Response callback function type.
        /// Used to return responses generated by the request handler
        /// function before serialization.
        using response_callback_type
            = std::function<void(std::optional<Response>)>;

        /// \brief Request handler callback function.
        /// Defines a function that processes an inbound request, attempts to
        /// generate a response, and passes that response to a callback for
        /// transmission. Should return false if processing the request failed
        /// and the server should return a general error.
        using callback_type
            = std::function<bool(Request, response_callback_type)>;

        /// Register a request handler callback function for processing
        /// requests, generating responses, and passing those responses
        /// to a response callback.
        /// \param callback function to register to process client requests and
        ///                 call the response callback with the response.
        void register_handler_callback(callback_type callback) {
            m_callback = std::move(callback);
        }

      protected:
        /// Deserializes an RPC request, then calls the registered request
        /// handler function. Provides the request handler function with a
        /// callback which serializes the response. That callback passes the
        /// serialized data buffer to the callback provided here for
        /// transmission.
        /// \param request_buf buffer holding an RPC request.
        /// \param response_callback callback which transmits the serialized
        ///                          response buffer.
        /// \return std::nullopt if the request handler reported that the
        ///         request started successfully. Also returns std::nullopt if
        ///         deserializing the request failed. Returns a serialized
        ///         error response if the request handler returns a failure.
        auto async_call(InBuffer request_buf,
                        std::function<void(cbdc::buffer)> response_callback)
            -> std::optional<OutBuffer> {
            if constexpr(!raw_mode) {
                auto req = server_type::deserialize_request(request_buf);
                if(!req.has_value()) {
                    return std::nullopt;
                }
                auto success = m_callback(
                    std::move(req.value().m_payload),
                    [&,
                     resp_cb = std::move(response_callback),
                     hdr
                     = req.value().m_header](std::optional<Response> resp) {
                        auto resp_buf
                            = server_type::serialize_response(std::move(hdr),
                                                              std::move(resp));
                        resp_cb(std::move(resp_buf));
                    });
                if(!success) {
                    return server_type::template serialize_response<Response>(
                        std::move(req.value().m_header),
                        std::nullopt);
                }
            } else {
                auto maybe_failure
                    = server_type::make_failure_response(request_buf);
                if(!maybe_failure.has_value()) {
                    return std::nullopt;
                }
                auto success
                    = m_callback(std::move(request_buf),
                                 [&,
                                  resp_cb = std::move(response_callback),
                                  fail_buf = maybe_failure.value()](
                                     std::optional<Response> resp) {
                                     if(resp.has_value()) {
                                         resp_cb(std::move(resp.value()));
                                     } else {
                                         resp_cb(std::move(fail_buf));
                                     }
                                 });
                if(!success) {
                    return maybe_failure;
                }
            }
            return std::nullopt;
        }

      private:
        callback_type m_callback;

        using server_type = server<Request, Response, InBuffer, OutBuffer>;

        static constexpr auto raw_mode
            = std::is_same_v<Request,
                             buffer> && std::is_same_v<Response, buffer>;
    };

    /// \brief Asynchronous pass-through RPC server.
    /// Skips serialization and deserialization. Passes buffers directly to
    /// callback functions for subclasses to forward or implement their own
    /// processing logic.
    using raw_async_server = async_server<buffer, buffer>;
}

#endif
