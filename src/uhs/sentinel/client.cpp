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
        -> std::optional<response> {
        return m_client.call(std::move(tx));
    }

    auto client::execute_transaction(
        transaction::full_tx tx,
        std::function<void(result_type)> result_callback) -> bool {
        return m_client.call(std::move(tx), std::move(result_callback));
    }
}
