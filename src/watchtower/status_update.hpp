// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_WATCHTOWER_STATUS_UPDATE_H_
#define OPENCBDC_TX_SRC_WATCHTOWER_STATUS_UPDATE_H_

#include "common/hashmap.hpp"
#include "status_update_messages.hpp"
#include "transaction/transaction.hpp"

#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace cbdc::watchtower {
    /// The current status of the Watchtower's progress in searching for a
    /// particular UHS ID.
    enum class search_status {
        /// The Watchtower has finished scanning the block history for the UHS
        /// ID in the request and hasn't found it.
        no_history,
        /// The UTXO set contains the requested UHS ID. The holder can spend
        /// the corresponding output.
        unspent,
        /// The STXO set contains the requested UHS ID. The corresponding
        /// output has already been spent and cannot be spent again.
        spent,
        /// The transaction processing system rejected the requested UHS ID's
        /// transaction due to a problem with another input.
        tx_rejected,
        /// The transaction processing system rejected the transaction because
        /// the requested UHS ID was already spent in another transaction or it
        /// did not exist in the first place.
        invalid_input,
        /// The transaction processing system failed to process the transaction
        /// containing the requested UHS ID due to an internal (non-client)
        /// issue. The transaction may be retried.
        internal_error
    };

    /// Set of UHS IDs to query, keyed by Tx IDs.
    using tx_id_uhs_ids = std::unordered_map<hash_t,
                                             std::vector<hash_t>,
                                             hashing::const_sip_hash<hash_t>>;

    /// Network request to interact with the Watchtower's status update
    /// service.
    class status_update_request {
      public:
        friend auto
        cbdc::operator<<(cbdc::serializer& packet,
                         const cbdc::watchtower::status_update_request& su_req)
            -> cbdc::serializer&;
        friend auto cbdc::operator>>(cbdc::serializer& packet,
                                     status_update_request& su_req)
            -> cbdc::serializer&;

        auto operator==(const status_update_request& rhs) const -> bool;

        status_update_request() = delete;

        /// Constructor.
        /// \param uhs_ids the UHS IDs for which the client would like to search, keyed by Tx ID.
        explicit status_update_request(tx_id_uhs_ids uhs_ids);

        /// Construct from a packet.
        /// \param pkt packet containing a serialized StatusUpdateRequest.
        explicit status_update_request(cbdc::serializer& pkt);

        /// UHS IDs for which the client would like to search.
        /// \return the UHS ID.
        [[nodiscard]] auto uhs_ids() const -> const tx_id_uhs_ids&;

      private:
        /// UHS ID for which the client would like to search.
        tx_id_uhs_ids m_uhs_ids;
    };

    /// Represents the internal state of an ongoing status update request.
    /// Returned in pertinent success responses.
    class status_update_state {
      public:
        friend auto
        cbdc::operator<<(cbdc::serializer& packet,
                         const cbdc::watchtower::status_update_state& state)
            -> cbdc::serializer&;
        friend auto
        cbdc::operator>>(cbdc::serializer& packet,
                         cbdc::watchtower::status_update_state& state)
            -> cbdc::serializer&;
        friend class status_request_check_success;

        auto operator==(const status_update_state& rhs) const -> bool;

        /// Constructor.
        /// \param status current status of a running search.
        /// \param uhs_id the UHS ID for which this status is valid.
        /// \param block_height the index of the block in which the watchtower found the transaction. Not valid if uhs_id is empty.
        status_update_state(search_status status,
                            uint64_t block_height,
                            hash_t uhs_id);

        /// Construct from a packet.
        /// \param pkt packet containing a serialized StatusUpdateState.
        explicit status_update_state(cbdc::serializer& pkt);

        /// Returns the current SearchStatus of the StatusUpdate.
        /// \return the current SearchStatus.
        [[nodiscard]] auto status() const -> search_status;

        /// Return the block height of the block containing the transaction in
        /// which the UHS ID was found. This value should only be considered
        /// valid if status() is spent or unspent.
        /// \return UHS ID block height.
        [[nodiscard]] auto block_height() const -> uint64_t;

        /// Returns the UHS ID for which the status is valid.
        /// \return UHS ID.
        [[nodiscard]] auto uhs_id() const -> hash_t;

      private:
        status_update_state() = default;

        search_status m_status{};
        uint64_t m_block_height{};
        hash_t m_uhs_id{};
    };

    /// Reported UHS ID states, keyed by Tx IDs.
    using tx_id_states = std::unordered_map<hash_t,
                                            std::vector<status_update_state>,
                                            hashing::const_sip_hash<hash_t>>;

    /// Indicates a successful check request, sent with a StatusUpdateResponse.
    /// \see cbdc::watchtower::StatusUpdateRequestCheck.
    class status_request_check_success {
      public:
        friend auto cbdc::operator<<(
            cbdc::serializer& packet,
            const cbdc::watchtower::status_request_check_success& chs)
            -> cbdc::serializer&;
        friend auto
        cbdc::operator>>(cbdc::serializer& packet,
                         cbdc::watchtower::status_request_check_success& chs)
            -> cbdc::serializer&;

        auto operator==(const status_request_check_success& rhs) const -> bool;

        status_request_check_success() = delete;

        /// Constructor.
        /// \param states current states of the subscription.
        explicit status_request_check_success(tx_id_states states);

        /// Construct from a packet.
        /// \param pkt packet containing a serialized StatusRequestCheckSuccess.
        explicit status_request_check_success(cbdc::serializer& pkt);

        /// Returns the states of a set of UHS IDs, following the order of the
        /// UHS IDs in the containing StatusUpdateResponse.
        /// \return subscription state info.
        /// \note make sure to preserve the backing object for as long as the returned reference is needed, or explicitly copy this result.
        [[nodiscard]] auto states() const -> const tx_id_states&;

      private:
        tx_id_states m_states;
    };
}

#endif // OPENCBDC_TX_SRC_WATCHTOWER_STATUS_UPDATE_H_
