// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "status_client.hpp"

#include "format.hpp"
#include "util/serialization/util2.hpp"

namespace cbdc::locking_shard::rpc {
    status_client::status_client(
        std::vector<std::vector<network::endpoint_t>>
            shard_read_only_endpoints,
        std::vector<config::shard_range_t> shard_ranges,
        std::chrono::milliseconds timeout)
        : m_shard_ranges(std::move(shard_ranges)),
          m_request_timeout(timeout) {
        assert(m_shard_ranges.size() == shard_read_only_endpoints.size());
        m_shard_clients.reserve(m_shard_ranges.size());
        for(auto& cluster : shard_read_only_endpoints) {
            m_shard_clients.emplace_back(
                std::make_unique<
                    cbdc::rpc::tcp_client<status_request, status_response>>(
                    std::move(cluster)));
        }
    }

    auto status_client::init() -> bool {
        for(auto& client : m_shard_clients) {
            if(!client->init()) {
                return false;
            }
        }
        return true;
    }

    auto status_client::check_tx_id(const hash_t& tx_id)
        -> std::optional<bool> {
        return make_request<tx_status_request>(tx_id);
    }

    auto status_client::check_unspent(const hash_t& uhs_id)
        -> std::optional<bool> {
        return make_request<uhs_status_request>(uhs_id);
    }
}
