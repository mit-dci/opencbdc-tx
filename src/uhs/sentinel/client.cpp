// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "client.hpp"

#include "uhs/sentinel/format.hpp"
#include "uhs/transaction/messages.hpp"
#include "util/serialization/util.hpp"

namespace cbdc::sentinel::rpc {
    client::client(std::vector<network::endpoint_t> endpoints,
                   std::shared_ptr<logging::log> logger)
        : m_logger(std::move(logger)),
          m_client(std::move(endpoints)) {}

    auto client::init() -> bool {
        if(!m_client.init()) {
            m_logger->error("Failed to initialize sentinel RPC client");
            return false;
        }
        return true;
    }

    auto client::execute_transaction(cbdc::transaction::full_tx tx)
        -> std::optional<execute_response> {
        auto res = m_client.call(execute_request{std::move(tx)});
        if(!res.has_value()) {
            return std::nullopt;
        }
        return std::get<execute_response>(res.value());
    }

    auto client::execute_transaction(
        transaction::full_tx tx,
        std::function<void(execute_result_type)> result_callback) -> bool {
        return m_client.call(
            execute_request{std::move(tx)},
            [cb = std::move(result_callback)](std::optional<response> res) {
                if(!res.has_value()) {
                    cb(std::nullopt);
                    return;
                }
                cb(std::get<execute_response>(res.value()));
            });
    }

    auto client::validate_transaction(cbdc::transaction::full_tx tx)
        -> std::optional<validate_response> {
        auto res = m_client.call(validate_request{std::move(tx)});
        if(!res.has_value()) {
            return std::nullopt;
        }
        return std::get<validate_response>(res.value());
    }

    auto client::validate_transaction(
        transaction::full_tx tx,
        std::function<void(validate_result_type)> result_callback) -> bool {
        return m_client.call(
            validate_request{std::move(tx)},
            [cb = std::move(result_callback)](std::optional<response> res) {
                if(!res.has_value()) {
                    cb(std::nullopt);
                    return;
                }
                cb(std::get<validate_response>(res.value()));
            });
    }
}
