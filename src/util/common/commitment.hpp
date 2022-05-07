// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COMMON_COMMITMENT_H_
#define OPENCBDC_TX_SRC_COMMON_COMMITMENT_H_

#include "hash.hpp"
#include "keys.hpp"

#include <optional>
#include <secp256k1_generator.h>

namespace cbdc {
    /// Creates a Pedersen commitment
    ///
    /// \param ctx secp256k1 context initialized for signing and commitment
    /// \param value the value to commit to
    /// \param blind a 32-byte blinding factor
    /// \return a pedersen commitment, or nullopt if making the commitment
    ///         failed
    auto
    commit(const secp256k1_context* ctx, uint64_t value, const hash_t& blind)
        -> std::optional<secp256k1_pedersen_commitment>;

    /// Serializes a Pedersen commitment
    ///
    /// \param ctx secp256k1 context initialized for signing and commitment
    /// \param comm the commitment to serialize
    /// \return a serialized pedersen commitment
    auto serialize_commitment(const secp256k1_context* ctx,
                              secp256k1_pedersen_commitment comm)
        -> commitment_t;

    /// Creates and serializes a Pedersen commitment
    ///
    /// A shortcut to to \ref commit and \ref serialize_commitment
    ///
    /// \param ctx secp256k1 context initialized for signing and commitment
    /// \param value the value to commit to
    /// \param blinder a 32-byte blinding factor
    /// \return a serialized pedersen commitment, or nullopt if making the
    ///         commitment failed
    auto make_commitment(const secp256k1_context* ctx,
                         uint64_t value,
                         const hash_t& blinder) -> std::optional<commitment_t>;

    /// Attempts to create a Pedersen commitment and serialize it into 32-bytes
    ///
    /// We assume the y-coordinate of a 32-byte Pedersen commitment is positve.
    /// If the blinder provided results in a Pedersen commitment with a
    /// postive y-coordinate, then this method will discard the leading byte
    /// (which normally disambiguates the y-coordinate) and return the
    /// remaining 32 bytes.
    ///
    /// If the resulting Pedersen commitment has a negative y-coordinate, then
    /// this method returns std::nullopt.
    ///
    /// Practically, you would expect to need to call this method multiple
    /// times (on average, twice) with different, random blinding factors to
    /// succeed in generating a 32-byte-representable Pedersen commitment.
    ///
    /// \param ctx secp256k1 context initialized for signing and commitment
    /// \param value the value to commit to
    /// \param blinder a 32-byte blinding factor
    /// \return a serialized pedersen commitment, or nullopt if the commitment
    ///         could not be represented in 32 bytes
    auto make_xonly_commitment(const secp256k1_context* ctx,
                               uint64_t value,
                               const hash_t& blinder) -> std::optional<hash_t>;

    /// Attempts to deserialize a Pedersen commitment
    ///
    /// \param ctx secp256k1 context initialized for signing and commitment
    /// \param comm a 33-byte serialized Pedersen commitment
    /// \return the deserialized commitment, or std::nullopt if the input
    ///         was invalid
    auto deserialize_commitment(const secp256k1_context* ctx,
                                commitment_t comm)
        -> std::optional<secp256k1_pedersen_commitment>;

    /// Attempts to deserialize an xonly Pedersen commitment
    ///
    /// \param ctx secp256k1 context initialized for signing and commitment
    /// \param comm a 32-byte serialized x-only Pedersen commitment
    /// \return the deserialized commitment, or std::nullopt if the input
    ///         was invalid
    auto expand_xonly_commitment(const secp256k1_context* ctx, hash_t& comm)
        -> std::optional<secp256k1_pedersen_commitment>;
}

#endif // OPENCBDC_TX_SRC_COMMON_COMMITMENT_H_
