// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_SERVER_H_
#define OPENCBDC_TX_SRC_RPC_SERVER_H_

#include "header.hpp"
#include "messages.hpp"
#include "util/raft/util.hpp"
#include "util/serialization/util.hpp"

#include <functional>
#include <optional>

namespace cbdc::rpc {
    /// Type to distinguish between servers that implement synchronous versus
    /// asynchronous request handling.
    enum class handler_type {
        blocking,
        async
    };

    /// Generic RPC server. Handles serialization of requests and responses.
    /// Subclass to implement request handling functionality.
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
    class server {
      public:
        server() = default;
        server(server&&) noexcept = default;
        auto operator=(server&&) noexcept -> server& = default;
        server(const server&) = default;
        auto operator=(const server&) -> server& = default;

        virtual ~server() = default;

        using request_type = request<Request>;
        using response_type = response<Response>;

      protected:
        /// Deserializes a request from a buffer.
        /// \tparam BufType type of buffer to deserialize from.
        /// \param request_buf buffer to deserialize.
        /// \return deserialized request object, or std::nullopt if
        ///         deserialization failed.
        template<typename BufType = InBuffer>
        auto deserialize_request(BufType& request_buf)
            -> std::optional<request_type> {
            return from_buffer<request_type>(request_buf);
        }

        /// Serialize a response into a buffer.
        /// \tparam type of response payload. Defaults to Response.
        /// \param request_header header from the corresponding request.
        /// \param response_payload payload to include in the response, or
        ///                         std::nullopt if the request failed.
        /// \return serialized response buffer.
        template<typename R = Response>
        auto
        serialize_response(header request_header,
                           std::optional<R> response_payload) -> OutBuffer {
            return make_buffer<response<R>, OutBuffer>(
                {request_header, response_payload});
        }

        /// Serialize a failure response buffer from the given request buffer.
        /// \param request_buf buffer containing a failed RPC request.
        /// \return serialized failure response, or std::nullopt if
        ///         deserializing the request failed.
        auto make_failure_response(cbdc::buffer& request_buf)
            -> std::optional<cbdc::buffer> {
            auto hdr = from_buffer<header>(request_buf);
            if(!hdr.has_value()) {
                return std::nullopt;
            }
            return serialize_response<null_response_type>(hdr.value(),
                                                          std::nullopt);
        }

      private:
        struct null_response_type {};
    };
}

#endif
