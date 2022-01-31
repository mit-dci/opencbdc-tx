// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "keys.hpp"

#include <cassert>
#include <secp256k1_schnorrsig.h>

namespace cbdc {
    auto pubkey_from_privkey(const privkey_t& privkey, secp256k1_context* ctx)
        -> pubkey_t {
        secp256k1_keypair keypair{};
        [[maybe_unused]] const auto create_ret
            = ::secp256k1_keypair_create(ctx, &keypair, privkey.data());
        assert(create_ret == 1);

        secp256k1_xonly_pubkey xpub{};
        [[maybe_unused]] const auto xonly_ret
            = ::secp256k1_keypair_xonly_pub(ctx, &xpub, nullptr, &keypair);
        assert(xonly_ret == 1);

        pubkey_t pubkey;
        [[maybe_unused]] const auto ser_ret
            = ::secp256k1_xonly_pubkey_serialize(ctx, pubkey.data(), &xpub);
        assert(ser_ret == 1);
        return pubkey;
    }
}
