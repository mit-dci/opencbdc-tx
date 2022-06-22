// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "commitment.hpp"

#include "crypto/sha256.h"
#include "keys.hpp"

#include <array>
#include <cstring>
#include <secp256k1_generator.h>
#include <sstream>
#include <vector>

namespace cbdc {
    auto
    commit(const secp256k1_context* ctx, uint64_t value, const hash_t& blind)
        -> std::optional<secp256k1_pedersen_commitment> {
        secp256k1_pedersen_commitment commit{};
        auto res = secp256k1_pedersen_commit(ctx,
                                             &commit,
                                             blind.data(),
                                             value,
                                             secp256k1_generator_h);
        if(res != 1) {
            return std::nullopt;
        }

        return commit;
    }

    auto serialize_commitment(const secp256k1_context* ctx,
                              secp256k1_pedersen_commitment comm)
        -> commitment_t {
        commitment_t c{};
        secp256k1_pedersen_commitment_serialize(ctx, c.data(), &comm);
        return c;
    }

    auto make_commitment(const secp256k1_context* ctx,
                         uint64_t value,
                         const hash_t& blind) -> std::optional<commitment_t> {
        auto comm = commit(ctx, value, blind);
        if(!comm.has_value()) {
            return std::nullopt;
        }

        return serialize_commitment(ctx, comm.value());
    }

    auto deserialize_commitment(const secp256k1_context* ctx,
                                commitment_t comm)
        -> std::optional<secp256k1_pedersen_commitment> {
        secp256k1_pedersen_commitment commitment{};
        if(secp256k1_pedersen_commitment_parse(ctx, &commitment, comm.data())
           != 1) {
            return std::nullopt;
        }

        return commitment;
    }
}
