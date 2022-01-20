// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_LOCKING_SHARD_CLIENT_H_
#define OPENCBDC_TX_SRC_LOCKING_SHARD_CLIENT_H_

#include "common/logging.hpp"
#include "interface.hpp"
#include "messages.hpp"
#include "rpc/tcp_client.hpp"

namespace cbdc::locking_shard::rpc {
    /// RPC client for the mutable interface to a locking shard raft cluster.
    class client final : public interface {
      public:
        /// Constructs a new locking shard client for issuing RPCs to a remote
        /// shard cluster. The class is thread-safe on a per-dtx ID basis; only
        /// one thread should issue RPCs for a given dtx ID at a time.
        /// \param endpoints vector of shard node endpoints comprising
        ///                  the cluster
        /// \param output_range inclusive range of UHS ID prefixes covered by
        ///                     the shard cluster
        /// \param logger log instance for writing status messages
        client(std::vector<network::endpoint_t> endpoints,
               const std::pair<uint8_t, uint8_t>& output_range,
               logging::log& logger);

        client() = delete;
        ~client() override;
        client(const client&) = delete;
        auto operator=(const client&) -> client& = delete;
        client(client&&) = delete;
        auto operator=(client&&) -> client& = delete;

        /// Initializes the RPC client. Connects to the shard cluster and
        /// starts the response handler thread.
        /// \return false if there is only one node in the cluster and
        ///         connecting to it failed.
        auto init() -> bool;

        /// Issues a lock RPC to the remote shard and returns its response.
        /// \param txs vector of txs representing the input and output UHS
        ///            IDs to lock for spending or creation
        /// \param dtx_id dtx ID for this batch of transactions
        /// \return if lock operation succeeds, a vector of flags indicating
        ///         which transactions in the batch had their outputs belonging
        ///         to the shard cluster locked
        auto lock_outputs(std::vector<tx>&& txs, const hash_t& dtx_id)
            -> std::optional<std::vector<bool>> override;

        /// Issues an apply RPC to the remote shard and returns its response.
        /// \param complete_txs vector of flags to indicate which transactions
        ///                     in the distributed transaction should be
        ///                     finalized or rolled back
        /// \param dtx_id dtx ID upon which to perform apply
        /// \return true if the apply operation succeeded
        auto apply_outputs(std::vector<bool>&& complete_txs,
                           const hash_t& dtx_id) -> bool override;

        /// Issues a discard RPC to the remote shard and returns its response.
        /// \param dtx_id dtx ID to discard
        /// \return true if the discard operation succeeded
        auto discard_dtx(const hash_t& dtx_id) -> bool override;

        /// Shuts down the client and unblocks any existing requests waiting
        /// for a response.
        void stop() override;

      private:
        auto send_request(const request& req) -> std::optional<response>;

        std::atomic_bool m_running{true};

        std::unique_ptr<cbdc::rpc::tcp_client<request, response>> m_client;

        logging::log& m_log;
    };
}

#endif // OPENCBDC_TX_SRC_LOCKING_SHARD_CLIENT_H_
