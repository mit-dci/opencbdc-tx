// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RPC_CLIENT_H_
#define OPENCBDC_TX_SRC_RPC_CLIENT_H_

#include "format.hpp"
#include "messages.hpp"
#include "util/serialization/util.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <optional>

namespace cbdc::rpc {
    /// Generic RPC client. Handles serialization of requests and responses
    /// combined with a message header. Subclass to define actual remote
    /// communication logic.
    /// \tparam Request type for requests.
    /// \tparam Response type for responses.
    template<typename Request, typename Response>
    class client {
      public:
        client() = default;
        client(client&&) noexcept = default;
        auto operator=(client&&) noexcept -> client& = default;
        client(const client&) = delete;
        auto operator=(const client&) -> client& = delete;

        virtual ~client() = default;

        using request_type = request<Request>;
        using response_type = response<Response>;

        /// User-provided response callback function type for asynchronous
        /// requests.
        using response_callback_type
            = std::function<void(std::optional<Response>)>;

        /// Issues the given request with an optional timeout, then waits for
        /// and returns the response. Serializes the request data, calls
        /// call_raw() to transmit the data and get a response, and returns the
        /// deserialized response. Thread safe.
        /// \param request_payload payload for the RPC.
        /// \param timeout optional timeout in milliseconds. Zero indicates the
        ///                call should not timeout.
        /// \return response from the RPC, or std::nullopt if the call timed out
        ///         or produced an error.
        [[nodiscard]] auto call(Request request_payload,
                                std::chrono::milliseconds timeout
                                = std::chrono::milliseconds::zero())
            -> std::optional<Response> {
            auto [request_buf, request_id]
                = make_request(std::move(request_payload));
            auto resp = call_raw(std::move(request_buf), request_id, timeout);
            if(!resp.has_value()) {
                return std::nullopt;
            }
            assert(resp.value().m_header.m_request_id == request_id);
            return resp.value().m_payload;
        }

        /// Issues an asynchronous request and registers the given callback to
        /// handle the response. Serializes the request data, then transmits it
        /// with call_raw(). Thread safe.
        /// \param request_payload payload for the RPC.
        /// \param response_callback function for the request handler to call
        ///                          when the response is available.
        /// \return true if the request was sent successfully.
        auto call(Request request_payload,
                  response_callback_type response_callback) -> bool {
            auto [request_buf, request_id]
                = make_request(std::move(request_payload));
            auto ret = call_raw(
                std::move(request_buf),
                request_id,
                [req_id = request_id, resp_cb = std::move(response_callback)](
                    std::optional<response_type> resp) {
                    if(!resp.has_value()) {
                        return;
                    }
                    assert(resp.value().m_header.m_request_id == req_id);
                    resp_cb(std::move(resp.value().m_payload));
                });
            return ret;
        }

      protected:
        /// Deserializes a response object from the given buffer.
        /// \param response_buf buffer containing an RPC response.
        /// \return response object or std::nullopt if deserialization failed.
        auto deserialize_response(cbdc::buffer& response_buf)
            -> std::optional<response_type> {
            return from_buffer<response_type>(response_buf);
        }

        /// Response callback function type for handling an RPC response.
        using raw_callback_type
            = std::function<void(std::optional<response_type>)>;

      private:
        std::atomic<uint64_t> m_current_request_id{};

        /// Subclasses must override this function to define the logic for
        /// call() to transmit a serialized RPC request and wait for a
        /// serialized response with an optional timeout.
        /// \param request_buf serialized request object.
        /// \param request_id identifier to match requests with responses.
        /// \param timeout timeout in milliseconds. Zero indicates the
        ///                call should not timeout.
        /// \return response object, or std::nullopt if sending the request
        ///         failed or the timeout expired while waiting for a response.
        virtual auto call_raw(cbdc::buffer request_buf,
                              request_id_type request_id,
                              std::chrono::milliseconds timeout)
            -> std::optional<response_type> = 0;

        virtual auto call_raw(cbdc::buffer request_buf,
                              request_id_type request_id,
                              raw_callback_type response_callback) -> bool
            = 0;

        auto make_request(Request request_payload)
            -> std::pair<cbdc::buffer, request_id_type> {
            auto request_id = m_current_request_id++;
            auto req = request_type{{request_id}, std::move(request_payload)};
            return {make_buffer(req), request_id};
        }
    };
}

#endif
