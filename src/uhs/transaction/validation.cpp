// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validation.hpp"

#include "transaction.hpp"

#include <cassert>
#include <memory>
#include <secp256k1.h>
#include <secp256k1_schnorrsig.h>
#include <set>

namespace cbdc::transaction::validation {
    using secp256k1_context_destroy_type = void (*)(secp256k1_context*);

    std::unique_ptr<secp256k1_context,
                    secp256k1_context_destroy_type>
        secp_context{secp256k1_context_create(SECP256K1_CONTEXT_NONE),
               &secp256k1_context_destroy};

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

    auto check_tx(const cbdc::transaction::full_tx& tx)
        -> std::optional<tx_error> {
        const auto structure_err = check_tx_structure(tx);
        if(structure_err) {
            return structure_err;
        }

        for(size_t idx = 0; idx < tx.m_inputs.size(); idx++) {
            const auto& inp = tx.m_inputs[idx];
            const auto input_err = check_input_structure(inp);
            if(input_err) {
                auto&& [code, data] = input_err.value();
                return tx_error{input_error{code, data, idx}};
            }
        }

        for(size_t idx = 0; idx < tx.m_outputs.size(); idx++) {
            const auto& out = tx.m_outputs[idx];
            const auto output_err = check_output_value(out);
            if(output_err) {
                return tx_error{output_error{output_err.value(), idx}};
            }
        }

        const auto in_out_set_error = check_in_out_set(tx);
        if(in_out_set_error) {
            return in_out_set_error;
        }

        for(size_t idx = 0; idx < tx.m_witness.size(); idx++) {
            const auto witness_err = check_witness(tx, idx);
            if(witness_err) {
                return tx_error{witness_error{witness_err.value(), idx}};
            }
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

    auto check_input_structure(const cbdc::transaction::input& inp)
        -> std::optional<
            std::pair<input_error_code, std::optional<output_error_code>>> {
        const auto data_err = check_output_value(inp.m_prevout_data);
        if(data_err) {
            return {{input_error_code::data_error, data_err}};
        }

        return std::nullopt;
    }

    auto check_in_out_set(const cbdc::transaction::full_tx& tx)
        -> std::optional<tx_error> {
        uint64_t input_total{0};
        for(const auto& inp : tx.m_inputs) {
            if(input_total + inp.m_prevout_data.m_value <= input_total) {
                return tx_error(tx_error_code::value_overflow);
            }
            input_total += inp.m_prevout_data.m_value;
        }

        uint64_t output_total{0};
        for(const auto& out : tx.m_outputs) {
            if(output_total + out.m_value <= output_total) {
                return tx_error(tx_error_code::value_overflow);
            }
            output_total += out.m_value;
        }

        if(input_total != output_total) {
            return tx_error(tx_error_code::asymmetric_values);
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
                                       sighash.size(),
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

    auto check_output_value(const cbdc::transaction::output& out)
        -> std::optional<output_error_code> {
        if(out.m_value < 1) {
            return output_error_code::zero_value;
        }

        return std::nullopt;
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
