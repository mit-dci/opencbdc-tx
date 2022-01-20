// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TX_STATUS_CLIENT_H_INC
#define TX_STATUS_CLIENT_H_INC

#include "common/config.hpp"
#include "rpc/tcp_client.hpp"
#include "status_interface.hpp"
#include "status_messages.hpp"

namespace cbdc::locking_shard::rpc {
    /// Client for interacting with the read-only port on 2PC shards. Allows
    /// for checking whether a TX ID has been confirmed or whether a UHS ID
    /// is currently unspent. Connects to all shard nodes to handle failover
    /// and routes requests to the relevant shard.
    class status_client : public status_interface {
      public:
        /// Constructor.
        /// \param shard_read_only_endpoints list of endpoints by shard ID
        ///                                  then node ID.
        /// \param shard_ranges list of shard hash prefix ranges by shard
        ///                     ID. Must be the same size as
        ///                     shard_read_only_endpoints.
        /// \param timeout optional timeout for status requests. Zero indicates
        ///                no timeout.
        status_client(std::vector<std::vector<network::endpoint_t>>
                          shard_read_only_endpoints,
                      std::vector<config::shard_range_t> shard_ranges,
                      std::chrono::milliseconds timeout
                      = std::chrono::milliseconds::zero());

        /// Destructor.
        ~status_client() override = default;

        status_client() = delete;
        status_client(const status_client&) = delete;
        auto operator=(const status_client&) -> status_client& = delete;
        status_client(status_client&&) = delete;
        auto operator=(status_client&&) -> status_client& = delete;

        /// Initializes the client by creating a TCP RPC client for each shard
        /// cluster.
        /// \return true if the RPC clients initialized successfully.
        auto init() -> bool;

        /// Queries the shard cluster responsible for the given UHS ID for
        /// whether it is unspent.
        /// \param uhs_id UHS ID to query.
        /// \return true if the UHS ID is unspent, or std::nullopt if the request
        ///         failed.
        [[nodiscard]] auto check_unspent(const hash_t& uhs_id)
            -> std::optional<bool> override;

        /// Queries the shard cluster responsible for the given TX ID for
        /// whether it is in the confirmed TX IDs cache.
        /// \param tx_id TX ID to query.
        /// \return true if the cache contains the TX ID, or std::nullopt if
        ///         the request failed.
        [[nodiscard]] auto check_tx_id(const hash_t& tx_id)
            -> std::optional<bool> override;

      private:
        std::vector<std::unique_ptr<
            cbdc::rpc::tcp_client<status_request, status_response>>>
            m_shard_clients;
        std::vector<config::shard_range_t> m_shard_ranges;
        std::chrono::milliseconds m_request_timeout;

        template<typename T>
        auto make_request(const hash_t& val) -> std::optional<bool> {
            // TODO: optimize the algorithm for shard selection.
            for(size_t i = 0; i < m_shard_ranges.size(); i++) {
                if(config::hash_in_shard_range(m_shard_ranges[i], val)) {
                    return m_shard_clients[i]->call(T{val}, m_request_timeout);
                }
            }
            return std::nullopt;
        }
    };
}

#endif
