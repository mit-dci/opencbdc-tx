// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validation.hpp"

#include "transaction.hpp"

#include <cassert>
#include <memory>
#include <secp256k1.h>
#include <secp256k1_generator.h>
#include <secp256k1_schnorrsig.h>
#include <set>

namespace cbdc::transaction::validation {
    static const auto secp_context
        = std::unique_ptr<secp256k1_context,
                          decltype(&secp256k1_context_destroy)>(
            secp256k1_context_create(SECP256K1_CONTEXT_SIGN
                                     | SECP256K1_CONTEXT_VERIFY),
            &secp256k1_context_destroy);

    struct GensDeleter {
        explicit GensDeleter(secp256k1_context* ctx) : m_ctx(ctx) {}

        void operator()(secp256k1_bulletproofs_generators* gens) const {
            secp256k1_bulletproofs_generators_destroy(m_ctx, gens);
        }

        secp256k1_context* m_ctx;
    };

    /// should be twice the bitcount of the range-proof's upper bound
    ///
    /// e.g., if proving things in the range [0, 2^64-1], it should be 128.
    static const auto inline generator_count = 128;

    static const std::unique_ptr<secp256k1_bulletproofs_generators,
                                 GensDeleter>
        generators{secp256k1_bulletproofs_generators_create(secp_context.get(),
                                                            generator_count),
                   GensDeleter(secp_context.get())};

    auto input_error::operator==(const input_error& rhs) const -> bool {
        return std::tie(m_code, m_data_err, m_idx)
            == std::tie(rhs.m_code, rhs.m_data_err, rhs.m_idx);
    }

    auto output_error::operator==(const output_error& rhs) const -> bool {
        return std::tie(m_code, m_idx) == std::tie(rhs.m_code, rhs.m_idx);
    }

    auto witness_error::operator==(const witness_error& rhs) const -> bool {
        return std::tie(m_code, m_idx) == std::tie(rhs.m_code, rhs.m_idx);
    }

    auto proof_error::operator==(const proof_error& rhs) const -> bool {
        return m_code == rhs.m_code;
    }

    auto check_tx(const cbdc::transaction::full_tx& tx)
        -> std::optional<tx_error> {
        const auto structure_err = check_tx_structure(tx);
        if(structure_err) {
            return structure_err;
        }

        for(size_t idx = 0; idx < tx.m_witness.size(); idx++) {
            const auto witness_err = check_witness(tx, idx);
            if(witness_err) {
                return tx_error{witness_error{witness_err.value(), idx}};
            }
        }

        std::vector<commitment_t> inputs{};
        inputs.reserve(tx.m_inputs.size());
        for(const auto& inp : tx.m_inputs) {
            inputs.push_back(inp.m_prevout_data.m_auxiliary);
        }

        const cbdc::transaction::compact_tx ctx(tx);
        const auto proof_error = check_proof(ctx, inputs);
        if(proof_error) {
            return proof_error;
        }

        return std::nullopt;
    }

    auto check_tx_structure(const cbdc::transaction::full_tx& tx)
        -> std::optional<tx_error> {
        const auto input_count_err = check_input_count(tx);
        if(input_count_err) {
            return input_count_err;
        }

        const auto output_count_err = check_output_count(tx);
        if(output_count_err) {
            return output_count_err;
        }

        const auto witness_count_err = check_witness_count(tx);
        if(witness_count_err) {
            return witness_count_err;
        }

        const auto input_set_err = check_input_set(tx);
        if(input_set_err) {
            return input_set_err;
        }

        return std::nullopt;
    }

    // TODO: check input assumptions with flags for whether preconditions have
    //       already been checked.
    auto check_witness(const cbdc::transaction::full_tx& tx, size_t idx)
        -> std::optional<witness_error_code> {
        const auto& witness_program = tx.m_witness[idx];
        if(witness_program.empty()) {
            return witness_error_code::missing_witness_program_type;
        }

        // Note this is safe because we aready checked the format in the prior
        // step
        const auto witness_program_type
            = static_cast<cbdc::transaction::validation::witness_program_type>(
                witness_program[0]);
        switch(witness_program_type) {
            case witness_program_type::p2pk:
                return check_p2pk_witness(tx, idx);
            default:
                return witness_error_code::unknown_witness_program_type;
        }
    }

    auto check_p2pk_witness(const cbdc::transaction::full_tx& tx, size_t idx)
        -> std::optional<witness_error_code> {
        const auto witness_len_err = check_p2pk_witness_len(tx, idx);
        if(witness_len_err) {
            return witness_len_err;
        }

        const auto witness_commitment_err
            = check_p2pk_witness_commitment(tx, idx);
        if(witness_commitment_err) {
            return witness_commitment_err;
        }

        const auto witness_sig_err = check_p2pk_witness_signature(tx, idx);
        if(witness_sig_err) {
            return witness_sig_err;
        }

        return std::nullopt;
    }

    auto check_p2pk_witness_len(const cbdc::transaction::full_tx& tx,
                                size_t idx)
        -> std::optional<witness_error_code> {
        const auto& wit = tx.m_witness[idx];
        if(wit.size() != p2pk_witness_len) {
            return witness_error_code::malformed;
        }

        return std::nullopt;
    }

    auto check_p2pk_witness_commitment(const cbdc::transaction::full_tx& tx,
                                       size_t idx)
        -> std::optional<witness_error_code> {
        const auto& wit = tx.m_witness[idx];
        const auto witness_program_hash
            = hash_data(wit.data(), p2pk_witness_prog_len);

        const auto& witness_program_commitment
            = tx.m_inputs[idx].m_prevout_data.m_witness_program_commitment;

        if(witness_program_hash != witness_program_commitment) {
            return witness_error_code::program_mismatch;
        }

        return std::nullopt;
    }

    auto check_p2pk_witness_signature(const cbdc::transaction::full_tx& tx,
                                      size_t idx)
        -> std::optional<witness_error_code> {
        const auto& wit = tx.m_witness[idx];
        secp256k1_xonly_pubkey pubkey{};

        // TODO: use C++20 std::span to avoid pointer arithmetic in validation
        // code
        pubkey_t pubkey_arr{};
        std::memcpy(pubkey_arr.data(),
                    &wit[sizeof(witness_program_type)],
                    sizeof(pubkey_arr));
        if(secp256k1_xonly_pubkey_parse(secp_context.get(),
                                        &pubkey,
                                        pubkey_arr.data())
           != 1) {
            return witness_error_code::invalid_public_key;
        }

        const auto sighash = cbdc::transaction::tx_id(tx);

        std::array<unsigned char, sig_len> sig_arr{};
        std::memcpy(sig_arr.data(),
                    &wit[p2pk_witness_prog_len],
                    sizeof(sig_arr));
        if(secp256k1_schnorrsig_verify(secp_context.get(),
                                       sig_arr.data(),
                                       sighash.data(),
                                       &pubkey)
           != 1) {
            return witness_error_code::invalid_signature;
        }

        return std::nullopt;
    }

    auto check_input_count(const cbdc::transaction::full_tx& tx)
        -> std::optional<tx_error> {
        if(tx.m_inputs.empty()) {
            return tx_error(tx_error_code::no_inputs);
        }

        return std::nullopt;
    }

    auto check_output_count(const cbdc::transaction::full_tx& tx)
        -> std::optional<tx_error> {
        if(tx.m_outputs.empty()) {
            return tx_error(tx_error_code::no_outputs);
        }

        return std::nullopt;
    }

    auto check_witness_count(const cbdc::transaction::full_tx& tx)
        -> std::optional<tx_error> {
        if(tx.m_inputs.size() != tx.m_witness.size()) {
            return tx_error(tx_error_code::missing_witness);
        }

        return std::nullopt;
    }

    auto check_input_set(const cbdc::transaction::full_tx& tx)
        -> std::optional<tx_error> {
        std::set<cbdc::transaction::out_point> inps;

        for(size_t idx = 0; idx < tx.m_inputs.size(); idx++) {
            const auto& inp = tx.m_inputs[idx];
            const auto it = inps.find(inp.m_prevout);
            if(it != inps.end()) {
                return tx_error{input_error{input_error_code::duplicate,
                                            std::nullopt,
                                            idx}};
            }
            inps.insert(inp.m_prevout);
        }

        return std::nullopt;
    }

    auto check_proof(const compact_tx& tx,
                     const std::vector<commitment_t>& inps)
        -> std::optional<proof_error> {
        auto* ctx = secp_context.get();
        static constexpr auto scratch_size = 100UL * 1024UL;
        secp256k1_scratch_space* scratch
            = secp256k1_scratch_space_create(ctx, scratch_size);
        std::vector<secp256k1_pedersen_commitment> in_comms{};
        for(const auto& comm : inps) {
            auto maybe_aux = deserialize_commitment(ctx, comm);
            if(!maybe_aux.has_value()) {
                return proof_error{proof_error_code::invalid_auxiliary};
            }
            auto aux = maybe_aux.value();
            in_comms.push_back(aux);

            // no need to validate range proofs for inputs
        }
        std::vector<secp256k1_pedersen_commitment> out_comms{};
        for(const auto& proof : tx.m_outputs) {
            auto maybe_aux = deserialize_commitment(ctx, proof.m_auxiliary);
            if(!maybe_aux.has_value()) {
                return proof_error{proof_error_code::invalid_auxiliary};
            }
            auto aux = maybe_aux.value();
            out_comms.push_back(aux);

            // todo: replace lower-bound with 1 instead of 0
            [[maybe_unused]] auto ret
                = secp256k1_bulletproofs_rangeproof_uncompressed_verify(
                    ctx,
                    scratch,
                    generators.get(),
                    secp256k1_generator_h,
                    proof.m_range.data(),
                    proof.m_range.size(),
                    0, // minimum
                    &aux,
                    nullptr, // extra commit
                    0        // extra commit length
                );

            if(ret != 1) {
                return proof_error{proof_error_code::out_of_range};
            }
        }

        if(!check_commitment_sum(in_comms, out_comms, 0)) {
            return proof_error{proof_error_code::wrong_sum};
        }

        return std::nullopt;
    }

    auto check_commitment_sum(
        const std::vector<secp256k1_pedersen_commitment>& inputs,
        const std::vector<secp256k1_pedersen_commitment>& outputs,
        uint64_t minted) -> bool {
        std::vector<const secp256k1_pedersen_commitment*> in_ptrs{};
        in_ptrs.reserve(inputs.size());
        for(const auto& c : inputs) {
            in_ptrs.push_back(&c);
        }

        std::vector<const secp256k1_pedersen_commitment*> out_ptrs{};
        out_ptrs.reserve(outputs.size());
        for(const auto& c : outputs) {
            out_ptrs.push_back(&c);
        }

        // if this is a minting transaction, we need to balance the minted
        // amount
        secp256k1_pedersen_commitment minting_com{};
        if(minted != 0) {
            minting_com = commit(secp_context.get(), minted, hash_t{}).value();
            out_ptrs.push_back(&minting_com);
        }

        return secp256k1_pedersen_verify_tally(secp_context.get(),
                                               in_ptrs.data(),
                                               in_ptrs.size(),
                                               out_ptrs.data(),
                                               out_ptrs.size())
            == 1;
    }

    auto get_p2pk_witness_commitment(const pubkey_t& payee) -> hash_t {
        auto witness_program = to_vector(payee);
        witness_program.insert(witness_program.begin(),
                               std::byte(witness_program_type::p2pk));

        const auto wit_commit
            = hash_data(witness_program.data(), witness_program.size());

        return wit_commit;
    }

    auto to_string(cbdc::transaction::validation::tx_error_code err)
        -> std::string {
        switch(err) {
            case cbdc::transaction::validation::tx_error_code::no_inputs:
                return "No inputs";
            case cbdc::transaction::validation::tx_error_code::no_outputs:
                return "No outputs";
            case cbdc::transaction::validation::tx_error_code::missing_witness:
                return "More inputs than witnesses";
            case cbdc::transaction::validation::tx_error_code::
                asymmetric_values:
                return "Input values do not equal output values";
            case cbdc::transaction::validation::tx_error_code::value_overflow:
                return "Total value of inputs or outputs overflows a 64-bit "
                       "integer";
            default:
                return "Unknown error";
        }
    }

    auto to_string(cbdc::transaction::validation::input_error_code err)
        -> std::string {
        switch(err) {
            case cbdc::transaction::validation::input_error_code::data_error:
                return "Prevout data error";
            case cbdc::transaction::validation::input_error_code::duplicate:
                return "Duplicate outpoint";
            default:
                return "Unknown error";
        }
    }

    auto to_string(cbdc::transaction::validation::output_error_code err)
        -> std::string {
        switch(err) {
            case output_error_code::zero_value:
                return "Output has zero value";
            default:
                return "Unknown error";
        }
    }

    auto to_string(const cbdc::transaction::validation::input_error& err)
        -> std::string {
        auto ret = "Input error (idx: " + std::to_string(err.m_idx)
                 + "): " + to_string(err.m_code);
        if(err.m_data_err.has_value()) {
            ret += ", Data error: " + to_string(err.m_data_err.value());
        }
        return ret;
    }

    auto to_string(const cbdc::transaction::validation::proof_error& err)
        -> std::string {
        switch(err.m_code) {
            case cbdc::transaction::validation::proof_error_code ::
                invalid_auxiliary:
                return "One or more auxiliary commitments were malformed";
            case cbdc::transaction::validation::proof_error_code ::
                invalid_uhs_id:
                return "One or more UHS ID commitments were malformed";
            case cbdc::transaction::validation::proof_error_code ::
                invalid_signature_key:
                return "Constructing the consistency-proof "
                       "verification key failed";
            case cbdc::transaction::validation::proof_error_code ::
                inconsistent_value:
                return "The values committed to by a UHS ID commitment and "
                       "its auxiliary commitment are not equal";
            case cbdc::transaction::validation::proof_error_code ::
                out_of_range:
                return "One or more output values lay outside their proven "
                       "range";
            case cbdc::transaction::validation::proof_error_code ::wrong_sum:
                return "Input values do not equal output values";
            default:
                return "Unknown error";
        }
    }

    auto to_string(cbdc::transaction::validation::witness_error_code err)
        -> std::string {
        switch(err) {
            case cbdc::transaction::validation::witness_error_code::malformed:
                return "Incorrect witness data length";
            case cbdc::transaction::validation::witness_error_code::
                missing_witness_program_type:
                return "Witness missing script type";
            case cbdc::transaction::validation::witness_error_code::
                program_mismatch:
                return "Witness commitment does not match witness program";
            case cbdc::transaction::validation::witness_error_code::
                invalid_signature:
                return "Witness signature is invalid";
            case cbdc::transaction::validation::witness_error_code::
                invalid_public_key:
                return "Witness public key is invalid";
            case cbdc::transaction::validation::witness_error_code::
                unknown_witness_program_type:
                return "Witness contains an unknown script type";
            default:
                return "Unknown error";
        }
    }

    auto to_string(const witness_error& err) -> std::string {
        return "Witness error (idx: " + std::to_string(err.m_idx)
             + "): " + to_string(err.m_code);
    }

    auto to_string(const output_error& err) -> std::string {
        return "Output error (idx: " + std::to_string(err.m_idx)
             + "): " + to_string(err.m_code);
    }

    auto to_string(const tx_error& err) -> std::string {
        auto str
            = std::visit(overloaded{[](const tx_error_code& e) {
                                        return "TX error: " + to_string(e);
                                    },
                                    [](const auto& e) {
                                        return to_string(e);
                                    }},
                         err);
        return str;
    }

    auto check_attestations(
        const transaction::compact_tx& tx,
        const std::unordered_set<pubkey_t, hashing::null>& pubkeys,
        size_t threshold) -> bool {
        if(tx.m_attestations.size() < threshold) {
            return false;
        }

        return std::all_of(tx.m_attestations.begin(),
                           tx.m_attestations.end(),
                           [&](const auto& att) {
                               return pubkeys.find(att.first) != pubkeys.end()
                                   && tx.verify(secp_context.get(), att);
                           });
    }
}
