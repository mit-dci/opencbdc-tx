// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CBDC_UNIVERSE0_SRC_3PC_AGENT_HTTP_SERVER_H_
#define CBDC_UNIVERSE0_SRC_3PC_AGENT_HTTP_SERVER_H_

#include "3pc/agent/runners/evm/impl.hpp"
#include "agent/impl.hpp"
#include "agent/server_interface.hpp"
#include "broker/interface.hpp"
#include "directory/interface.hpp"
#include "messages.hpp"
#include "util/common/blocking_queue.hpp"
#include "util/common/thread_pool.hpp"
#include "util/rpc/http/json_rpc_http_server.hpp"

#include <atomic>
#include <secp256k1.h>
#include <thread>

namespace cbdc::threepc::agent::rpc {
    /// RPC server for a agent. Manages retrying function execution if it fails
    /// due to a transient error.
    class http_server : public server_interface {
      public:
        /// Type alias for the underlying RPC server.
        using server_type = cbdc::rpc::json_rpc_http_server;

        /// Constructor. Registers the agent implementation with the
        /// RPC server using a request handler callback.
        /// \param srv pointer to an HTTP JSON-RPC server.
        /// \param broker broker instance.
        /// \param log log instance.
        /// \param cfg system configuration options.
        http_server(std::unique_ptr<server_type> srv,
                    std::shared_ptr<broker::interface> broker,
                    std::shared_ptr<logging::log> log,
                    const cbdc::threepc::config& cfg);

        /// Stops listening for incoming connections, waits for existing
        /// connections to drain.
        ~http_server() override;

        /// Starts listening for incoming connections and processing requests.
        /// \return true if listening was sucessful.
        auto init() -> bool override;

        http_server(const http_server&) = delete;
        auto operator=(const http_server&) -> http_server& = delete;
        http_server(http_server&&) = delete;
        auto operator=(http_server&&) -> http_server& = delete;

      private:
        std::unique_ptr<server_type> m_srv;

        enum error_code : int {
            wallet_not_supported = -32602,
            mining_not_supported = -32604,
            time_travel_not_supported = -32605,
            compiler_not_supported = -32606,
            coinbase_not_supported = -32607,
            uncles_not_supported = -32001,
            unknown_method = -32699,
            internal_error = -32002,
            not_found = -32001,
            invalid_address = -32201,
            invalid_topic = -32202,
            from_block_after_to = -32204,
            invalid_block_parameter = -32203,
            block_range_too_large = -32205,
            invalid_transaction_index = -32003,
            invalid_block_identifier = -32302,
            execution_error = -33000,
        };

        auto request_handler(const std::string& method,
                             const Json::Value& params,
                             const server_type::result_callback_type& callback)
            -> bool;

        auto handle_send_raw_transaction(
            Json::Value params,
            const server_type::result_callback_type& callback) -> bool;

        auto handle_get_transaction_count(
            Json::Value params,
            const server_type::result_callback_type& callback) -> bool;

        static auto
        handle_chain_id(const Json::Value& params,
                        const server_type::result_callback_type& callback)
            -> bool;

        auto handle_call(Json::Value params,
                         const server_type::result_callback_type& callback)
            -> bool;

        auto handle_send_transaction(
            Json::Value params,
            const server_type::result_callback_type& callback) -> bool;

        static auto
        handle_estimate_gas(const Json::Value& params,
                            const server_type::result_callback_type& callback)
            -> bool;

        static auto handle_client_version(
            const Json::Value& params,
            const server_type::result_callback_type& callback) -> bool;
        static auto
        handle_gas_price(const Json::Value& params,
                         const server_type::result_callback_type& callback)
            -> bool;

        auto handle_get_code(Json::Value params,
                             const server_type::result_callback_type& callback)
            -> bool;
        auto
        handle_get_balance(Json::Value params,
                           const server_type::result_callback_type& callback)
            -> bool;
        static auto
        handle_accounts(const Json::Value& params,
                        const server_type::result_callback_type& callback)
            -> bool;

        auto handle_get_transaction_by_hash(
            Json::Value params,
            const server_type::result_callback_type& callback) -> bool;

        auto handle_get_transaction_receipt(
            Json::Value params,
            const server_type::result_callback_type& callback) -> bool;

        auto
        handle_not_supported(Json::Value params,
                             const server_type::result_callback_type& callback)
            -> bool;

        auto
        handle_block_number(const Json::Value& params,
                            const server_type::result_callback_type& callback)
            -> bool;
        auto
        handle_get_block(Json::Value params,
                         const server_type::result_callback_type& callback)
            -> bool;
        auto handle_get_block_txcount(
            Json::Value params,
            const server_type::result_callback_type& callback) -> bool;
        auto
        handle_get_block_tx(Json::Value params,
                            const server_type::result_callback_type& callback)
            -> bool;
        auto
        handle_fee_history(Json::Value params,
                           const server_type::result_callback_type& callback)
            -> bool;
        auto handle_get_logs(Json::Value params,
                             const server_type::result_callback_type& callback)
            -> bool;
        auto handle_get_storage_at(
            Json::Value params,
            const server_type::result_callback_type& callback) -> bool;

        auto handle_sha3(Json::Value params,
                         const server_type::result_callback_type& callback)
            -> bool;

        static auto
        handle_error(const Json::Value& params,
                     const server_type::result_callback_type& callback,
                     int code,
                     const std::string& message) -> bool;

        static auto
        handle_number(const Json::Value& params,
                      const server_type::result_callback_type& callback,
                      uint64_t number) -> bool;

        static auto
        handle_boolean(const Json::Value& params,
                       const server_type::result_callback_type& callback,
                       bool result) -> bool;

        auto handle_decode_raw_transaction(
            Json::Value params,
            const server_type::result_callback_type& callback) -> bool;

        auto
        parse_evm_log_query(const Json::Value& params,
                            const server_type::result_callback_type& callback)
            -> std::optional<threepc::agent::runner::evm_log_query>;

        auto fetch_block(Json::Value params,
                         const std::function<void(interface::exec_return_type,
                                                  cbdc::buffer)>& res_cb)
            -> bool;

        auto exec_tx(runner::evm_runner_function f_type,
                     cbdc::buffer& runner_params,
                     bool is_readonly_run,
                     std::function<void(interface::exec_return_type)> res_cb)
            -> bool;

        auto
        handle_unsupported(const std::string& method,
                           const Json::Value& params,
                           const server_type::result_callback_type& callback)
            -> bool;

        auto
        handle_supported(const std::string& method,
                         const Json::Value& params,
                         const server_type::result_callback_type& callback)
            -> std::optional<bool>;

        auto handle_static(const std::string& method,
                           const Json::Value& params,
                           const server_type::result_callback_type& callback)
            -> std::optional<bool>;

        static auto extract_evm_log_query_addresses(
            Json::Value params,
            const server_type::result_callback_type& callback,
            runner::evm_log_query& qry) -> bool;

        static auto extract_evm_log_query_topics(
            Json::Value params,
            const server_type::result_callback_type& callback,
            runner::evm_log_query& qry) -> bool;

        auto extract_evm_log_query_block(
            Json::Value params,
            const server_type::result_callback_type& callback,
            runner::evm_log_query& qry) -> bool;

        static void handle_get_logs_result(
            const server_type::result_callback_type& callback,
            const cbdc::buffer& runner_params,
            const runner::evm_log_query& qry,
            interface::exec_return_type res);
    };
}

#endif
