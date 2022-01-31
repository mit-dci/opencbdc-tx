// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_TRANSACTION_MESSAGES_H_
#define OPENCBDC_TX_SRC_TRANSACTION_MESSAGES_H_

#include "transaction.hpp"
#include "util/serialization/serializer.hpp"
#include "validation.hpp"

namespace cbdc {
    /// \brief Serializes an out_point.
    ///
    /// First serializes the transaction id, and then the index of the
    /// identified output in that transaction's output vector.
    /// \see \ref cbdc::operator<<(serializer&, const std::array<T, len>&)
    /// \see \ref cbdc::operator<<(serializer&, T)
    auto operator<<(serializer& packet, const transaction::out_point& op)
        -> serializer&;

    /// Deserializes an out_point.
    /// \see \ref cbdc::operator<<(serializer&, const transaction::out_point&)
    auto operator>>(serializer& packet, transaction::out_point& op)
        -> serializer&;

    /// \brief Serializes an output.
    ///
    /// Serializes the witness program commitment, and then the value.
    /// \see \ref cbdc::operator<<(serializer&, const std::array<T, len>&)
    /// \see \ref cbdc::operator<<(serializer&, T)
    auto operator<<(serializer& packet, const transaction::output& out)
        -> serializer&;

    /// Deserializes an output.
    /// \see \ref cbdc::operator<<(serializer&, const transaction::output&)
    auto operator>>(serializer& packet, transaction::output& out)
        -> serializer&;

    /// \brief Serializes an input.
    ///
    /// Serializes the out_point and then the output.
    /// \see \ref cbdc::operator<<(serializer&, const transaction::out_point&)
    /// \see \ref cbdc::operator<<(serializer&, const transaction::output&)
    auto operator<<(serializer& packet, const transaction::input& inp)
        -> serializer&;

    /// Deserializes an input.
    /// \see \ref cbdc::operator<<(serializer&, const transaction::input&)
    auto operator>>(serializer& packet, transaction::input& inp)
        -> serializer&;

    /// \brief Serializes a full transaction.
    ///
    /// Serializes the inputs, then the outputs, and then the witnesses.
    /// \see \ref cbdc::operator<<(serializer&, const std::vector<T>&)
    /// \see \ref cbdc::operator<<(serializer&, const transaction::input&)
    /// \see \ref cbdc::operator<<(serializer&, const transaction::output&)
    /// \see \ref cbdc::operator<<(serializer&, const std::byte)
    auto operator<<(serializer& packet, const transaction::full_tx& tx)
        -> serializer&;

    /// Deserializes a full transaction.
    /// \see \ref cbdc::operator<<(serializer&, const transaction::full_tx&)
    auto operator>>(serializer& packet, transaction::full_tx& tx)
        -> serializer&;

    /// \brief Serializes a compact transaction.
    ///
    /// Serializes the transaction id, then the input hashes,
    /// and then the output hashes.
    /// \see \ref cbdc::operator<<(serializer&, const std::array<T, len>&)
    /// \see \ref cbdc::operator<<(serializer&, const std::vector<T>&)
    auto operator<<(serializer& packet, const transaction::compact_tx& tx)
        -> serializer&;

    /// Deserializes a compact transaction.
    /// \see \ref cbdc::operator<<(serializer&, const transaction::compact_tx&)
    auto operator>>(serializer& packet, transaction::compact_tx& tx)
        -> serializer&;

    /// Deserializes an input error.
    /// \see \ref cbdc::operator<<(serializer&,
    ///           const transaction::validation::input_error&)
    auto operator>>(serializer& packet,
                    transaction::validation::input_error& e) -> serializer&;

    /// \brief Serializes an input error.
    ///
    /// Serializes the error code, then the optional error data, and then the
    /// input's index.
    /// \see \ref cbdc::operator<<(serializer&, T)
    /// \see \ref cbdc::operator<<(serializer&, const std::optional<T>&)
    auto operator<<(serializer& packet,
                    const transaction::validation::input_error& e)
        -> serializer&;

    /// Deserializes an output error.
    /// \see \ref cbdc::operator<<(serializer&,
    ///           const transaction::validation::output_error&)
    auto operator>>(serializer& packet,
                    transaction::validation::output_error& e) -> serializer&;

    /// \brief Serializes an output error.
    ///
    /// Serializes the error code and then the output's index.
    /// \see \ref cbdc::operator<<(serializer&, T)
    auto operator<<(serializer& packet,
                    const transaction::validation::output_error& e)
        -> serializer&;

    /// Deserializes a witness error.
    /// \see \ref cbdc::operator<<(serializer&,
    ///           const transaction::validation::witness_error&)
    auto operator>>(serializer& packet,
                    transaction::validation::witness_error& e) -> serializer&;

    /// \brief Serializes a witness error.
    ///
    /// Serializes the error code and then the witness's index.
    /// \see \ref cbdc::operator<<(serializer&, T)
    auto operator<<(serializer& packet,
                    const transaction::validation::witness_error& e)
        -> serializer&;
}

#endif // OPENCBDC_TX_SRC_TRANSACTION_MESSAGES_H_
