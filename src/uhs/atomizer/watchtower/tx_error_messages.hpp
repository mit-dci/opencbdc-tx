// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/** \file tx_error_messages.hpp
 * Messages atomizers and shards can use to transmit errors to the watchtower,
 * and which the watchtower can use to communicate errors to clients.
 */

#ifndef OPENCBDC_TX_SRC_WATCHTOWER_TX_ERROR_MESSAGES_H_
#define OPENCBDC_TX_SRC_WATCHTOWER_TX_ERROR_MESSAGES_H_

#include "uhs/transaction/transaction.hpp"
#include "util/common/hashmap.hpp"
#include "util/common/variant_overloaded.hpp"
#include "util/serialization/serializer.hpp"

#include <memory>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace cbdc::watchtower {
    struct tx_error_sync;
    class tx_error_inputs_dne;
    struct tx_error_stxo_range;
    struct tx_error_incomplete;
    class tx_error_inputs_spent;

    using tx_error_info = std::variant<tx_error_sync,
                                       tx_error_inputs_dne,
                                       tx_error_stxo_range,
                                       tx_error_incomplete,
                                       tx_error_inputs_spent>;

    class tx_error;
}

namespace cbdc {
    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::tx_error_inputs_dne& err)
        -> cbdc::serializer&;
    auto operator>>(cbdc::serializer& packet,
                    cbdc::watchtower::tx_error_inputs_dne& err)
        -> cbdc::serializer&;
    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::tx_error_inputs_spent& err)
        -> cbdc::serializer&;
    auto operator>>(cbdc::serializer& packet,
                    cbdc::watchtower::tx_error_inputs_spent& err)
        -> cbdc::serializer&;
    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::tx_error& err)
        -> cbdc::serializer&;
    auto operator>>(cbdc::serializer& packet, cbdc::watchtower::tx_error& err)
        -> cbdc::serializer&;
}

namespace cbdc::watchtower {
    /// Indicates a shard that tried to process a given transaction was out of
    /// sync with the atomizer, and the transaction should be retried.
    struct tx_error_sync {
        tx_error_sync() = default;

        /// Construct from a packet.
        /// \param pkt packet containing a serialized tx_error_sync.
        explicit tx_error_sync(cbdc::serializer& pkt);

        auto operator==(const tx_error_sync& rhs) const -> bool;
    };

    /// Indicates a shard that tried to process a given transaction could not
    /// locate one or more of the transaction's inputs it expected to possess.
    class tx_error_inputs_dne {
      public:
        friend auto
        cbdc::operator<<(cbdc::serializer& packet,
                         const cbdc::watchtower::tx_error_inputs_dne& err)
            -> cbdc::serializer&;
        friend auto
        cbdc::operator>>(cbdc::serializer& packet,
                         cbdc::watchtower::tx_error_inputs_dne& err)
            -> cbdc::serializer&;

        auto operator==(const tx_error_inputs_dne& rhs) const -> bool;

        tx_error_inputs_dne() = delete;

        /// Constructor.
        /// \param input_uhs_ids the UHS IDs of the inputs that caused this error.
        explicit tx_error_inputs_dne(std::vector<hash_t> input_uhs_ids);

        /// Construct from a packet.
        /// \param pkt packet containing a serialized tx_error_input_dne.
        explicit tx_error_inputs_dne(cbdc::serializer& pkt);

        /// Returns the UHS IDs of the inputs that caused this error.
        /// \return input UHS ID set.
        [[nodiscard]] auto input_uhs_ids() const -> std::vector<hash_t>;

      private:
        std::vector<hash_t> m_input_uhs_ids;
    };

    /// Indicates that a shard did not attest to this transaction recently
    /// enough for the atomizer to check it against the STXO cache.
    struct tx_error_stxo_range {
        tx_error_stxo_range() = default;

        /// Construct from a packet.
        /// \param pkt packet containing a serialized tx_error_stxo_range.
        explicit tx_error_stxo_range(cbdc::serializer& pkt);

        auto operator==(const tx_error_stxo_range& rhs) const -> bool;
    };

    /// Indicates that the atomizer did not receive enough attestations for a
    /// particular transaction from shards before it had to clean up the
    /// transaction and free up space for others.
    struct tx_error_incomplete {
        tx_error_incomplete() = default;

        /// Construct from a packet.
        /// \param pkt packet containing a serialized tx_error_incomplete.
        explicit tx_error_incomplete(cbdc::serializer& pkt);

        auto operator==(const tx_error_incomplete& rhs) const -> bool;
    };

    /// Indicates that the given transaction contains one or more inputs that
    /// have already been spent in other transactions sent to the atomizer.
    class tx_error_inputs_spent {
      public:
        friend auto
        cbdc::operator<<(cbdc::serializer& packet,
                         const cbdc::watchtower::tx_error_inputs_spent& err)
            -> cbdc::serializer&;
        friend auto
        cbdc::operator>>(cbdc::serializer& packet,
                         cbdc::watchtower::tx_error_inputs_spent& err)
            -> cbdc::serializer&;

        auto operator==(const tx_error_inputs_spent& rhs) const -> bool;

        tx_error_inputs_spent() = delete;

        /// Constructor.
        /// \param input_uhs_ids the UHS IDs of the inputs that caused this error.
        explicit tx_error_inputs_spent(
            std::unordered_set<hash_t, hashing::null> input_uhs_ids);

        /// Construct from a packet.
        /// \param pkt packet containing a serialized tx_error_inputs_spent.
        explicit tx_error_inputs_spent(cbdc::serializer& pkt);

        /// Returns the UHS IDs of the inputs that caused this error.
        /// \return input UHS ID set.
        [[nodiscard]] auto input_uhs_ids() const
            -> std::unordered_set<hash_t, hashing::null>;

      private:
        std::unordered_set<hash_t, hashing::null> m_input_uhs_ids;
    };

    /// Wrapper for transaction errors
    class tx_error {
      public:
        friend auto cbdc::operator<<(cbdc::serializer& packet,
                                     const cbdc::watchtower::tx_error& err)
            -> cbdc::serializer&;
        friend auto cbdc::operator>>(cbdc::serializer& packet,
                                     cbdc::watchtower::tx_error& err)
            -> cbdc::serializer&;

        auto operator==(const tx_error& rhs) const -> bool;

        tx_error() = delete;

        /// Sync error constructor.
        /// \param tx_id the transaction ID to which this error pertains.
        /// \param err sync error.
        tx_error(const hash_t& tx_id, const tx_error_sync& err);

        /// Input does-not-exist error constructor.
        /// \param tx_id the transaction ID to which this error pertains.
        /// \param err does-not-exist error.
        tx_error(const hash_t& tx_id, const tx_error_inputs_dne& err);

        /// STXO range error constructor.
        /// \param tx_id the transaction ID to which this error pertains.
        /// \param err STXO range error.
        tx_error(const hash_t& tx_id, const tx_error_stxo_range& err);

        /// Incomplete error constructor.
        /// \param tx_id the transaction ID to which this error pertains.
        /// \param err incomplete error.
        tx_error(const hash_t& tx_id, const tx_error_incomplete& err);

        /// Inputs spent error constructor.
        /// \param tx_id the transaction ID to which this error pertains.
        /// \param err inputs spent error.
        tx_error(const hash_t& tx_id, const tx_error_inputs_spent& err);

        /// Construct from a packet.
        /// \param pkt packet containing a serialized tx_error_inputs_spent.
        explicit tx_error(cbdc::serializer& pkt);

        /// Returns the transaction ID to which this error pertains.
        /// \return Tx ID.
        [[nodiscard]] auto tx_id() const -> hash_t;

        /// Returns the type and associated information about this error.
        /// \return error information.
        [[nodiscard]] auto info() const -> tx_error_info;

        /// Returns a human-friendly description of the error.
        /// \return error description.
        [[nodiscard]] auto to_string() const -> std::string;

      private:
        hash_t m_tx_id{};
        std::shared_ptr<tx_error_info> m_info;
    };
}

#endif // OPENCBDC_TX_SRC_WATCHTOWER_TX_ERROR_MESSAGES_H_
