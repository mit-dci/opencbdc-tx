// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_BLOCKING_SERVER_H_
#define OPENCBDC_TX_SRC_RPC_BLOCKING_SERVER_H_

#include "server.hpp"

namespace cbdc::rpc {
    /// Generic synchronous RPC server. Handles serialization of requests and
    /// responses. Dispatches incoming requests to a handler callback for
    /// processing. Subclass to define specific remote communication logic.
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
    class blocking_server
        : public server<Request, Response, InBuffer, OutBuffer> {
      public:
        blocking_server() = default;
        blocking_server(blocking_server&&) noexcept = default;
        auto operator=(blocking_server&&) noexcept
            -> blocking_server& = default;
        blocking_server(const blocking_server&) = default;
        auto operator=(const blocking_server&) -> blocking_server& = default;

        ~blocking_server() override = default;

        static constexpr handler_type handler = handler_type::blocking;

        /// Handler callback function type which accepts a request and returns
        /// a response, or returns std::nullopt if it encounters an error while
        /// processing the request.
        using callback_type = std::function<std::optional<Response>(Request)>;

        /// Register a handler callback function for processing requests and
        /// returning responses.
        /// \param callback function to register to process client requests and
        ///                 return responses.
        void register_handler_callback(callback_type callback) {
            m_callback = std::move(callback);
        }

      protected:
        /// Synchronously deserializes an RPC request, calls the request
        /// handler function, then serializes and returns the response.
        /// \param request_buf buffer holding an RPC request.
        /// \return serialized response, or std::nullopt if deserializing or
        ///         handling the request failed.
        auto blocking_call(InBuffer request_buf) -> std::optional<OutBuffer> {
            auto req = server_type::deserialize_request(request_buf);
            if(!req.has_value()) {
                return std::nullopt;
            }
            auto resp = std::optional<Response>();
            if(m_callback) {
                resp = m_callback(std::move(req.value().m_payload));
            }
            return server_type::serialize_response(
                std::move(req.value().m_header),
                std::move(resp));
        }

      private:
        callback_type m_callback;

        using server_type = server<Request, Response, InBuffer, OutBuffer>;
    };
}

#endif
