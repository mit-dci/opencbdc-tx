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

    output::output(hash_t witness_program_commitment, uint64_t value)
        : m_witness_program_commitment(witness_program_commitment),
          m_value(value) {}

    compact_output::compact_output(const commitment_t& aux,
                                   const rangeproof_t& range,
                                   const hash_t& provenance)
        : m_value_commitment(aux),
          m_range(range),
          m_provenance(provenance) {}

    auto compact_output::operator==(const compact_output& rhs) const -> bool {
        return m_value_commitment == rhs.m_value_commitment
            && m_range == rhs.m_range
            && m_provenance == rhs.m_provenance;
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

    auto calculate_uhs_id(const compact_output& put) -> hash_t {
        CSHA256 sha;
        sha.Write(put.m_provenance.data(), put.m_provenance.size());
        sha.Write(put.m_value_commitment.data(), put.m_value_commitment.size());
        // sha.Write(put.m_range.data(), put.m_range.size());

        hash_t id{};
        sha.Finalize(id.data());

        return id;
    }

    auto roll_auxiliaries(secp256k1_context* ctx,
                          random_source& rng,
                          const std::vector<hash_t>& blinds,
                          std::vector<spend_data>& out_spend_data)
        -> std::vector<secp256k1_pedersen_commitment> {
        const auto make_public = blinds.empty();
        const hash_t empty{};

        std::vector<secp256k1_pedersen_commitment> auxiliaries{};

        std::vector<hash_t> new_blinds{};
        for(uint64_t i = 0; i < out_spend_data.size() - 1; ++i) {
            while(true) {
                auto rprime = make_public ? empty : rng.random_hash();
                auto commitment
                    = commit(ctx, out_spend_data[i].m_value, rprime);
                if(commitment.has_value()) {
                    auxiliaries.push_back(commitment.value());
                    new_blinds.push_back(rprime);
                    out_spend_data[i].m_blind = rprime;
                    break;
                }
            }
        }

        if(!make_public) {
            std::vector<hash_t> allblinds{blinds};
            std::copy(new_blinds.begin(),
                      new_blinds.end(),
                      std::back_inserter(allblinds));

            std::vector<const unsigned char*> blind_ptrs;
            blind_ptrs.reserve(allblinds.size());
            for(const auto& b : allblinds) {
                blind_ptrs.push_back(b.data());
            }

            hash_t last_blind{};
            [[maybe_unused]] auto ret
                = secp256k1_pedersen_blind_sum(ctx,
                                               last_blind.data(),
                                               blind_ptrs.data(),
                                               allblinds.size(),
                                               blinds.size());
            assert(ret == 1);
            auxiliaries.push_back(
                commit(ctx, out_spend_data.back().m_value, last_blind)
                    .value());
            out_spend_data.back().m_blind = last_blind;
        } else {
            auxiliaries.push_back(
                commit(ctx, out_spend_data.back().m_value, empty).value());
            new_blinds.push_back(empty);
            out_spend_data.back().m_blind = empty;
        }

        return auxiliaries;
    }

    auto prove(secp256k1_context* ctx,
               secp256k1_bppp_generators* gens,
               random_source& rng,
               const spend_data& out_spend_data,
               const secp256k1_pedersen_commitment* comm) -> rangeproof_t {

        static constexpr auto scratch_size = 100UL * 1024UL;
        secp256k1_scratch_space* scratch
            = secp256k1_scratch_space_create(ctx, scratch_size);

        static constexpr auto upper_bound = 64; // 2^64 - 1
        static constexpr auto base = 16;

        rangeproof_t range{};
        size_t rangelen
            = secp256k1_bppp_rangeproof_proof_length(ctx, upper_bound, base);
        range.assign(rangelen, 0);

        [[maybe_unused]] auto ret
            = secp256k1_bppp_rangeproof_prove(
                ctx,
                scratch,
                gens,
                secp256k1_generator_h,
                range.data(),
                &rangelen,
                upper_bound,
                base,
                out_spend_data.m_value,
                1,
                comm, // the commitment for this output
                out_spend_data.m_blind.data(),
                rng.random_hash().data(),
                nullptr, // extra_commit
                0        // extra_commit length
            );

        secp256k1_scratch_space_destroy(ctx, scratch);

        assert(ret == 1);

        return range;
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
            = secp256k1_schnorrsig_sign32(ctx,
                                          sig.data(),
                                          payload.data(),
                                          &keypair,
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
                                       payload.size(),
                                       &pubkey)
           != 1) {
            return false;
        }

        return true;
    }
}
