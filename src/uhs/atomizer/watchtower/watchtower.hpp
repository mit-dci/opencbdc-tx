// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/** \file watchtower.hpp
 * Watchtower core functionality.
 */

#ifndef OPENCBDC_TX_SRC_WATCHTOWER_WATCHTOWER_H_
#define OPENCBDC_TX_SRC_WATCHTOWER_WATCHTOWER_H_

#include "block_cache.hpp"
#include "error_cache.hpp"
#include "messages.hpp"
#include "status_update.hpp"

namespace cbdc::watchtower {
    /// Request the watchtower's known best block height.
    struct best_block_height_request {
        auto operator==(const best_block_height_request& /* unused */) const
            -> bool;

        best_block_height_request() = default;

        /// Construct from a packet.
        /// \param pkt packet containing a serialized BestBlockHeightRequest.
        explicit best_block_height_request(cbdc::serializer& pkt);
    };

    /// Contains the watchtower's known best block height.
    /// \see cbdc::watchtower::BestBlockHeightRequest.
    class best_block_height_response {
      public:
        friend auto cbdc::operator<<(
            cbdc::serializer& packet,
            const cbdc::watchtower::best_block_height_response& bbh_res)
            -> cbdc::serializer&;
        friend auto
        cbdc::operator>>(cbdc::serializer& packet,
                         cbdc::watchtower::best_block_height_response& bbh_res)
            -> cbdc::serializer&;

        auto operator==(const best_block_height_response& rhs) const -> bool;

        best_block_height_response() = delete;

        /// Constructor.
        /// \param height block height to include in the response.
        explicit best_block_height_response(uint64_t height);

        /// Construct from a packet.
        /// \param pkt packet containing a serialized BestBlockHeightResponse.
        explicit best_block_height_response(cbdc::serializer& pkt);

        /// Returns the states of a set of UHS IDs, following the order of the
        /// UHS IDs in the containing StatusUpdateResponse.
        /// \return the watchtower's best block height.
        [[nodiscard]] auto height() const -> uint64_t;

      private:
        uint64_t m_height{};
    };

    /// RPC request message to the watchtower external endpoint.
    class request {
      public:
        friend auto cbdc::operator<<(cbdc::serializer& packet,
                                     const cbdc::watchtower::request& req)
            -> cbdc::serializer&;

        auto operator==(const request& rhs) const -> bool;

        request() = delete;

        using request_t
            = std::variant<status_update_request, best_block_height_request>;

        /// Constructor.
        /// \param req request payload.
        explicit request(request_t req);

        /// Construct from a packet.
        /// \param pkt packet containing a serialized request.
        explicit request(cbdc::serializer& pkt);

        /// Return the request payload.
        /// \return request payload.
        [[nodiscard]] auto payload() const -> const request_t&;

      private:
        request_t m_req;
    };

    /// RPC response message from the watchtower external endpoint.
    class response {
      public:
        friend auto cbdc::operator<<(cbdc::serializer& packet,
                                     const cbdc::watchtower::response& res)
            -> cbdc::serializer&;

        auto operator==(const response& rhs) const -> bool;

        response() = delete;

        using response_t = std::variant<status_request_check_success,
                                        best_block_height_response>;

        /// Constructor.
        /// \param resp response payload.
        explicit response(response_t resp);

        /// Construct from a packet.
        /// \param pkt packet containing a serialized response.
        explicit response(cbdc::serializer& pkt);

        /// Return the response payload.
        /// \return response payload.
        [[nodiscard]] auto payload() const -> const response_t&;

      private:
        response_t m_resp;
    };

    /// Service to answer client requests for processing status updates on
    /// submitted transactions.
    class watchtower {
      public:
        watchtower() = delete;

        /// Constructor.
        /// \param block_cache_size the number of blocks to store in this Watchtower's block cache.
        /// \param error_cache_size the number of errors to store in this Watchtower's error cache.
        /// \see cbdc::watchtower::BlockCache
        watchtower(size_t block_cache_size, size_t error_cache_size);

        /// Adds a new block from the Atomizer to the Watchtower. Currently
        /// just forwards the block to the in-memory cache to await requests
        /// from clients.
        /// \param blk block to add.
        void add_block(cbdc::atomizer::block&& blk);

        /// Adds an error from an internal component to the Watchtower's error
        /// cache.
        /// \param errs error to add.
        void add_errors(std::vector<tx_error>&& errs);

        /// Composes a response to a status update request based on the data
        /// available. Currently only supports check requests against blocks
        /// cached in-memory.
        ///
        /// Error responses:
        /// - If the system raised an internal error while handling the
        ///   transaction, every UHS ID state is marked with the InternalError
        ///   status.
        /// - If the transaction was rejected because one of the UHS ID inputs
        ///   had already been spent, or because it could not be found in any
        ///   shards, the UHS ID states corresponding to problematic inputs are
        ///   marked with the InvalidInput status. All other requested inputs
        ///   are marked with the TxRejected status.
        /// \param req a status update request from a client.
        /// \return the response to send to the client or nullopt if request is invalid.
        auto handle_status_update_request(const status_update_request& req)
            -> std::unique_ptr<response>;

        /// Composes a response to a status update best block height request.
        /// \param req a best block height request from a client.
        /// \return the response to send to the client or nullopt if request is invalid.
        auto
        handle_best_block_height_request(const best_block_height_request& req)
            -> std::unique_ptr<response>;

      private:
        block_cache m_bc;
        std::shared_mutex m_bc_mut;
        error_cache m_ec;
        std::shared_mutex m_ec_mut;

        auto check_uhs_id_statuses(const std::vector<hash_t>& uhs_ids,
                                   const hash_t& tx_id,
                                   bool internal_err,
                                   bool tx_err,
                                   uint64_t best_height)
            -> std::vector<status_update_state>;
    };
}

#endif // OPENCBDC_TX_SRC_WATCHTOWER_WATCHTOWER_H_
