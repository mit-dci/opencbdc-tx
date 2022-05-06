// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transaction.hpp"

#include "crypto/sha256.h"
#include "messages.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/util.hpp"

namespace cbdc::transaction {
    auto out_point::operator==(const out_point& rhs) const -> bool {
        return m_tx_id == rhs.m_tx_id && m_index == rhs.m_index;
    }

    auto out_point::operator<(const out_point& rhs) const -> bool {
        return std::tie(m_tx_id, m_index) < std::tie(rhs.m_tx_id, rhs.m_index);
    }

    out_point::out_point(const hash_t& hash, const uint64_t index)
        : m_tx_id(hash),
          m_index(index) {}

    auto output::operator==(const output& rhs) const -> bool {
        return m_witness_program_commitment == rhs.m_witness_program_commitment
            && m_value == rhs.m_value;
    }

    auto output::operator!=(const output& rhs) const -> bool {
        return !(*this == rhs);
    }

    compact_output::compact_output(const hash_t& id, const commitment_t& aux,
        const rangeproof_t<>& range, const signature_t& consist)
        : m_id(id), m_auxiliary(aux), m_range(range), m_consistency(consist) {}

    auto compact_output::operator==(const compact_output& rhs) const -> bool {
        return m_id == rhs.m_id
            && m_auxiliary == rhs.m_auxiliary
            && m_range == rhs.m_range
            && m_consistency == rhs.m_consistency;
    }

    auto compact_output::operator!=(const compact_output& rhs) const -> bool {
        return !(*this == rhs);
    }

    auto input::operator==(const input& rhs) const -> bool {
        return m_prevout == rhs.m_prevout
            && m_prevout_data == rhs.m_prevout_data;
    }

    auto input::operator!=(const input& rhs) const -> bool {
        return !(*this == rhs);
    }

    auto input::hash() const -> hash_t {
        auto buf = cbdc::make_buffer(*this);

        CSHA256 sha;
        hash_t result;

        sha.Write(buf.c_ptr(), buf.size());
        sha.Finalize(result.data());

        return result;
    }

    auto full_tx::operator==(const full_tx& rhs) const -> bool {
        return rhs.m_inputs == m_inputs && rhs.m_outputs == m_outputs
            && rhs.m_witness == m_witness;
    }

    auto compact_tx_hasher::operator()(const compact_tx& tx) const noexcept
        -> size_t {
        size_t ret{};
        std::memcpy(&ret, tx.m_id.data(), sizeof(ret));
        return ret;
    }

    auto tx_id(const full_tx& tx) noexcept -> hash_t {
        CSHA256 sha;

        auto inp_buf = cbdc::make_buffer(tx.m_inputs);
        sha.Write(inp_buf.c_ptr(), inp_buf.size());

        auto out_buf = cbdc::make_buffer(tx.m_outputs);
        sha.Write(out_buf.c_ptr(), out_buf.size());

        hash_t ret;
        sha.Finalize(ret.data());

        return ret;
    }

    auto input_from_output(const full_tx& tx, size_t i, const hash_t& txid)
        -> std::optional<input> {
        input ret;
        if(i >= tx.m_outputs.size()) {
            return std::nullopt;
        }
        ret.m_prevout_data = tx.m_outputs[i];
        ret.m_prevout.m_index = i;
        ret.m_prevout.m_tx_id = txid;
        return ret;
    }

    auto input_from_output(const full_tx& tx, size_t i)
        -> std::optional<input> {
        const auto id = tx_id(tx);
        return input_from_output(tx, i, id);
    }

    auto uhs_id_from_output(const hash_t& entropy,
                            uint64_t i,
                            const output& output) -> hash_t {
        CSHA256 sha;
        hash_t ret;
        sha.Write(entropy.data(), entropy.size());
        std::array<unsigned char, sizeof(i)> index_arr{};
        std::memcpy(index_arr.data(), &i, sizeof(i));
        sha.Write(index_arr.data(), sizeof(i));

        auto buf = cbdc::make_buffer(output);

        sha.Write(buf.c_ptr(), buf.size());
        sha.Finalize(ret.data());
        return ret;
    }

    auto compact_tx::operator==(const compact_tx& tx) const noexcept -> bool {
        return m_id == tx.m_id;
    }

    compact_tx::compact_tx(const full_tx& tx) {
        m_id = tx_id(tx);
        for(const auto& inp : tx.m_inputs) {
            m_inputs.push_back(inp.hash());
        }
        for(uint64_t i = 0; i < tx.m_outputs.size(); i++) {
            m_uhs_outputs.push_back(
                uhs_id_from_output(m_id, i, tx.m_outputs[i]));
        }
    }

    auto compact_tx::sign(secp256k1_context* ctx, const privkey_t& key) const
        -> sentinel_attestation {
        auto payload = hash();
        auto pubkey = pubkey_from_privkey(key, ctx);
        secp256k1_keypair keypair{};
        [[maybe_unused]] const auto ret
            = secp256k1_keypair_create(ctx, &keypair, key.data());
        assert(ret == 1);

        auto sig = signature_t();
        [[maybe_unused]] const auto sign_ret
            = secp256k1_schnorrsig_sign(ctx,
                                        sig.data(),
                                        payload.data(),
                                        &keypair,
                                        nullptr,
                                        nullptr);
        assert(sign_ret == 1);
        return {pubkey, sig};
    }

    auto compact_tx::hash() const -> hash_t {
        // Don't include the attesations in the hash
        auto ctx = *this;
        ctx.m_attestations.clear();
        auto buf = make_buffer(ctx);
        auto sha = CSHA256();
        sha.Write(buf.c_ptr(), buf.size());
        auto ret = hash_t();
        sha.Finalize(ret.data());
        return ret;
    }

    auto compact_tx::verify(secp256k1_context* ctx,
                            const sentinel_attestation& att) const -> bool {
        auto payload = hash();
        secp256k1_xonly_pubkey pubkey{};
        if(secp256k1_xonly_pubkey_parse(ctx, &pubkey, att.first.data()) != 1) {
            return false;
        }

        if(secp256k1_schnorrsig_verify(ctx,
                                       att.second.data(),
                                       payload.data(),
                                       &pubkey)
           != 1) {
            return false;
        }

        return true;
    }
}
