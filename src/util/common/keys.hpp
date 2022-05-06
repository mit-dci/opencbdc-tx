// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COMMON_KEYS_H_
#define OPENCBDC_TX_SRC_COMMON_KEYS_H_

#include <array>
#include <cstring>
#include <vector>

#include <secp256k1_bulletproofs.h>

struct secp256k1_context_struct;
using secp256k1_context = struct secp256k1_context_struct;

namespace cbdc {
    /// Size of public keys used throughout the system, in bytes.
    static constexpr size_t pubkey_len = 32;
    /// Size of signatures used throughout the system, in bytes.
    static constexpr size_t sig_len = 64;
    /// Size of a standard, compressed EC Point
    static constexpr size_t point_len = 33;

    /// A private key of a public/private keypair.
    using privkey_t = std::array<unsigned char, pubkey_len>;
    /// A public key of a public/private keypair.
    using pubkey_t = std::array<unsigned char, pubkey_len>;
    /// A witness commitment.
    using witness_t = std::vector<std::byte>;
    /// A signature.
    using signature_t = std::array<unsigned char, sig_len>;
    /// A Pedersen Commitment.
    using commitment_t = std::array<unsigned char, point_len>;

    /// A range-proof
    /// \tparam N the size (in bytes) of the proof (dependent on the range
    ///           being proven.
    template <size_t N = SECP256K1_BULLETPROOFS_RANGEPROOF_UNCOMPRESSED_MAX_LENGTH_>
    using rangeproof_t = std::array<unsigned char, N>;

    /// Generates a public key from the specified private key.
    /// \param privkey private key for which to generate the public key.
    /// \param ctx the secp context to use.
    /// \return the public key.
    auto pubkey_from_privkey(const privkey_t& privkey, secp256k1_context* ctx)
        -> pubkey_t;

    /// Converts an std::array into an std::vector of the same size via copy.
    /// \param arr the array to convert.
    /// \return a vector containing the same data as the array.
    template<size_t S>
    auto to_vector(const std::array<unsigned char, S>& arr)
        -> std::vector<std::byte> {
        std::vector<std::byte> ret(S);
        memcpy(ret.data(), arr.data(), S);
        return ret;
    }
}

#endif // OPENCBDC_TX_SRC_COMMON_KEYS_H_
