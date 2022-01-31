// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "client.hpp"

#include "uhs/transaction/messages.hpp"

namespace cbdc::coordinator::rpc {
    client::client(std::vector<network::endpoint_t> endpoints)
        : m_client(std::make_unique<decltype(m_client)::element_type>(
            std::move(endpoints))) {}

    auto client::init() -> bool {
        return m_client->init();
    }

    auto client::execute_transaction(transaction::compact_tx tx,
                                     callback_type result_callback) -> bool {
        return m_client->call(std::move(tx), std::move(result_callback));
    }
}
