// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signature.hpp"

#include "address.hpp"
#include "crypto/sha256.h"
#include "format.hpp"
#include "hash.hpp"
#include "rlp.hpp"
#include "serialization.hpp"
#include "util.hpp"
#include "util/common/hash.hpp"
#include "util/serialization/util.hpp"

#include <optional>
#include <secp256k1.h>

namespace cbdc::parsec::agent::runner {
    auto secp256k1_ecdsa_recoverable_signature_to_evm_sig(
        secp256k1_ecdsa_recoverable_signature& sig,
        evm_tx_type type,
        uint64_t chain_id) -> evm_sig {
        auto esig = evm_sig{};

        std::reverse_copy(&sig.data[0],
                          &sig.data[sizeof(esig.m_r.bytes)],
                          esig.m_r.bytes);
        std::reverse_copy(
            &sig.data[sizeof(esig.m_r.bytes)],
            &sig.data[sizeof(esig.m_r.bytes) + sizeof(esig.m_s.bytes)],
            esig.m_s.bytes);

        uint8_t v{};
        std::reverse_copy(
            &sig.data[sizeof(esig.m_r.bytes) + sizeof(esig.m_s.bytes)],
            &sig.data[sizeof(esig.m_r.bytes) + sizeof(esig.m_s.bytes)
                      + sizeof(v)],
            &v);

        uint64_t v_large = v;
        if(type == evm_tx_type::legacy) {
            // Mutate v based on EIP155 and chain ID
            v_large += eip155_v_offset;
            v_large += (chain_id * 2);
        }
        esig.m_v = evmc::uint256be(v_large);

        return esig;
    }

    auto evm_sig_to_secp256k1_ecdsa_recoverable_signature(const evm_sig& esig,
                                                          evm_tx_type type,
                                                          uint64_t chain_id)
        -> std::optional<secp256k1_ecdsa_recoverable_signature> {
        auto sig = secp256k1_ecdsa_recoverable_signature{};

        std::reverse_copy(esig.m_r.bytes,
                          &esig.m_r.bytes[sizeof(esig.m_r.bytes)],
                          sig.data);
        std::reverse_copy(esig.m_s.bytes,
                          &esig.m_s.bytes[sizeof(esig.m_s.bytes)],
                          &sig.data[sizeof(esig.m_r.bytes)]);

        // Recover v mutation based on EIP155 and chain ID
        auto v_large = to_uint64(esig.m_v);
        if(type == evm_tx_type::legacy && v_large > eip155_v_offset) {
            v_large -= eip155_v_offset;
            v_large -= (chain_id * 2);
            if(v_large > std::numeric_limits<uint8_t>::max()) {
                return std::nullopt;
            }
        } else if(type == evm_tx_type::legacy
                  && v_large >= pre_eip155_v_offset) {
            v_large -= pre_eip155_v_offset;
        }
        auto v = static_cast<uint8_t>(v_large);
        std::memcpy(&sig.data[sizeof(sig.data) - sizeof(v)], &v, sizeof(v));

        return sig;
    }

    auto eth_sign(const privkey_t& key,
                  hash_t& hash,
                  evm_tx_type type,
                  const std::shared_ptr<secp256k1_context>& ctx,
                  uint64_t chain_id) -> evm_sig {
        secp256k1_ecdsa_recoverable_signature sig;
        [[maybe_unused]] const auto sig_ret = secp256k1_ecdsa_sign_recoverable(
            ctx.get(),
            &sig,
            hash.data(),
            key.data(),
            secp256k1_nonce_function_rfc6979,
            nullptr);
        assert(sig_ret == 1);
        return secp256k1_ecdsa_recoverable_signature_to_evm_sig(sig,
                                                                type,
                                                                chain_id);
    }

    auto check_signature(const cbdc::parsec::agent::runner::evm_tx& tx,
                         const std::shared_ptr<secp256k1_context>& ctx,
                         uint64_t chain_id) -> std::optional<evmc::address> {
        auto sighash = sig_hash(tx, chain_id);

        auto maybe_sig
            = evm_sig_to_secp256k1_ecdsa_recoverable_signature(tx.m_sig,
                                                               tx.m_type,
                                                               chain_id);

        if(!maybe_sig.has_value()) {
            return std::nullopt;
        }

        auto sig = maybe_sig.value();

        // Recover pubkey
        auto pk = std::make_unique<secp256k1_pubkey>();
        [[maybe_unused]] const auto rec_ret
            = secp256k1_ecdsa_recover(ctx.get(),
                                      pk.get(),
                                      &sig,
                                      sighash.data());
        if(rec_ret != 1) {
            return std::nullopt;
        }

        return eth_addr(pk, ctx);
    }

    auto sig_hash(const cbdc::parsec::agent::runner::evm_tx& tx,
                  uint64_t chain_id) -> hash_t {
        auto rlp_buf = tx_encode(tx, chain_id, true);
        return cbdc::keccak_data(rlp_buf.data(), rlp_buf.size());
    }
}
