// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "address.hpp"

#include "crypto/sha256.h"
#include "format.hpp"
#include "hash.hpp"
#include "rlp.hpp"
#include "util.hpp"
#include "util/common/hash.hpp"
#include "util/serialization/util.hpp"

#include <optional>
#include <secp256k1.h>

namespace cbdc::threepc::agent::runner {
    auto contract_address(const evmc::address& sender,
                          const evmc::uint256be& nonce) -> evmc::address {
        auto new_addr = evmc::address();
        auto buf = make_buffer(make_rlp_array(make_rlp_value(sender),
                                              make_rlp_value(nonce, true)));
        auto addr_hash = keccak_data(buf.data(), buf.size());
        constexpr auto addr_offset = addr_hash.size() - sizeof(new_addr.bytes);
        std::memcpy(new_addr.bytes,
                    addr_hash.data() + addr_offset,
                    sizeof(new_addr.bytes));
        return new_addr;
    }

    auto contract_address2(const evmc::address& sender,
                           const evmc::bytes32& salt,
                           const cbdc::hash_t& bytecode_hash)
        -> evmc::address {
        // Specs: https://eips.ethereum.org/EIPS/eip-1014

        auto new_addr = evmc::address();
        auto buf = cbdc::buffer();
        static constexpr uint8_t contract_address2_preimage_prefix = 0xFF;
        auto b = std::byte(contract_address2_preimage_prefix);
        buf.append(&b, sizeof(b));
        buf.append(sender.bytes, sizeof(sender.bytes));
        buf.append(salt.bytes, sizeof(salt.bytes));
        buf.append(bytecode_hash.data(), bytecode_hash.size());

        auto addr_hash = keccak_data(buf.data(), buf.size());
        constexpr auto addr_offset = addr_hash.size() - sizeof(new_addr.bytes);
        std::memcpy(new_addr.bytes,
                    addr_hash.data() + addr_offset,
                    sizeof(new_addr.bytes));
        return new_addr;
    }

    auto eth_addr(const std::unique_ptr<secp256k1_pubkey>& pk,
                  const std::shared_ptr<secp256k1_context>& ctx)
        -> evmc::address {
        static constexpr int uncompressed_pubkey_len = 65;
        auto pubkey_serialized
            = std::array<unsigned char, uncompressed_pubkey_len>();
        auto pubkey_size = pubkey_serialized.size();
        [[maybe_unused]] const auto ser_ret
            = ::secp256k1_ec_pubkey_serialize(ctx.get(),
                                              pubkey_serialized.data(),
                                              &pubkey_size,
                                              pk.get(),
                                              SECP256K1_EC_UNCOMPRESSED);
        assert(ser_ret == 1);

        auto pubkey_buffer = cbdc::buffer();
        pubkey_buffer.extend(uncompressed_pubkey_len);
        std::memcpy(pubkey_buffer.data(),
                    pubkey_serialized.data(),
                    pubkey_serialized.size());

        auto addr_hash = cbdc::keccak_data(pubkey_buffer.data_at(1),
                                           uncompressed_pubkey_len - 1);
        auto addr = evmc::address();
        constexpr auto addr_offset = addr_hash.size() - sizeof(addr.bytes);
        std::memcpy(addr.bytes,
                    addr_hash.data() + addr_offset,
                    sizeof(addr.bytes));
        return addr;
    }

    auto eth_addr(const cbdc::privkey_t& key,
                  const std::shared_ptr<secp256k1_context>& ctx)
        -> evmc::address {
        auto pk = std::make_unique<secp256k1_pubkey>();
        [[maybe_unused]] const auto pub_ret
            = ::secp256k1_ec_pubkey_create(ctx.get(), pk.get(), key.data());
        assert(pub_ret == 1);
        return eth_addr(pk, ctx);
    }
}
