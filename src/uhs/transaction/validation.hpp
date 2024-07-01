// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_TRANSACTION_VALIDATION_H_
#define OPENCBDC_TX_SRC_TRANSACTION_VALIDATION_H_

#include "transaction.hpp"

#include <cassert>
#include <memory>
#include <optional>
#include <secp256k1.h>
#include <secp256k1_schnorrsig.h>
#include <set>
#include <variant>

namespace cbdc::transaction::validation {
    /// Specifies how validators should interpret the witness program
    enum class witness_program_type : uint8_t {
        p2pk = 0x0 ///< Pay to Public Key
    };

    static constexpr auto p2pk_witness_prog_len
        = sizeof(witness_program_type) + sizeof(pubkey_t);
    static constexpr auto p2pk_witness_len = p2pk_witness_prog_len + sig_len;

    /// Types of input validation errors
    enum class input_error_code : uint8_t {
        duplicate,
        ///< More than one transaction input contains the same output
        data_error ///< A transaction input includes invalid output data
    };

    /// A transaction input validation error
    enum class output_error_code : uint8_t {
        zero_value ///< The output's value is 0
    };

    /// \brief An error that may occur when sentinels validate inputs
    ///
    /// Some input errors may require additional information which sentinels
    /// will encode in input_error::m_data_err
    struct input_error {
        auto operator==(const input_error& rhs) const -> bool;

        /// The type of input error
        input_error_code m_code{};

        /// Additional error information
        std::optional<output_error_code> m_data_err;

        /// The index of the input in the transaction
        uint64_t m_idx{};
    };

    /// A proof verification error
    enum class proof_error_code : uint8_t {
        invalid_commitment, ///< deserializing the auxiliary commitment failed
        invalid_uhs_id,     ///< deserializing the UHS ID failed
        out_of_range,          ///< range proof did not verify
        wrong_sum,             ///< auxiliaries did not sum as-required
        missing_rangeproof,    ///< rangeproof missing in output
    };

    /// An error that may occur when verifying transaction proof
    struct proof_error {
        auto operator==(const proof_error& rhs) const -> bool;

        /// The type of proof error
        proof_error_code m_code{};
    };

    /// Types of errors that may occur when sentinels validate witness
    /// commitments
    enum class witness_error_code : uint8_t {
        missing_witness_program_type, ///< The witness did not provide a
                                      ///< witness_program_type
        unknown_witness_program_type,
        ///< The validation system does not recognize the provided
        ///< witness_program_type
        malformed, ///< The witness's format appears invalid
        program_mismatch,
        ///< The witness's specified program doesn't match its commitment
        invalid_public_key, ///< The witness's public key is invalid
        invalid_signature   ///< The witness's signature is invalid
    };

    /// Types of errors that may occur when a sentinel statically validates a
    /// transaction
    enum class tx_error_code : uint8_t {
        no_inputs,       ///< There are no inputs
        no_outputs,      ///< There are no outputs
        missing_witness, ///< The number of witnesses and inputs do not match
        asymmetric_values,
        ///< The total values of inputs and outputs do not match
        value_overflow,
        ///< The total value of inputs/outputs overflows a 64-bit integer
    };

    /// An error that may occur when sentinels validate witness commitments
    struct witness_error {
        auto operator==(const witness_error& rhs) const -> bool;

        /// The type of witness error
        witness_error_code m_code{};

        /// The index of the witness in the transaction
        uint64_t m_idx{};
    };

    /// An error that may occur when sentinels validate transaction outputs
    struct output_error {
        auto operator==(const output_error& rhs) const -> bool;

        /// The type of output error
        output_error_code m_code{};

        /// The index of the output in the transaction
        uint64_t m_idx{};
    };

    /// \brief An error that may occur when sentinels or clients statically
    ///        validate a transaction
    ///
    /// A transaction can fail validation because of an error in the inputs,
    /// outputs, witnesses, or because the transaction-local invariants
    /// do not hold.
    using tx_error = std::variant<input_error,
                                  output_error,
                                  witness_error,
                                  tx_error_code,
                                  proof_error>;

    /// \brief Runs static validation checks on the given transaction
    ///
    /// \note This function returns immediately on the first-found error.
    ///
    /// \param tx transaction to validate
    /// \return null if transaction is valid, otherwise error information
    auto check_tx(const transaction::full_tx& tx) -> std::optional<tx_error>;
    auto check_tx_structure(const transaction::full_tx& tx)
        -> std::optional<tx_error>;
    // TODO: check input assumptions with flags for whether preconditions have
    //       already been checked.
    auto check_witness(const transaction::full_tx& tx, size_t idx)
        -> std::optional<witness_error_code>;
    auto check_p2pk_witness(const transaction::full_tx& tx, size_t idx)
        -> std::optional<witness_error_code>;
    auto check_p2pk_witness_len(const transaction::full_tx& tx, size_t idx)
        -> std::optional<witness_error_code>;
    auto check_p2pk_witness_commitment(const transaction::full_tx& tx,
                                       size_t idx)
        -> std::optional<witness_error_code>;
    auto check_p2pk_witness_signature(const transaction::full_tx& tx,
                                      size_t idx)
        -> std::optional<witness_error_code>;
    auto check_input_count(const transaction::full_tx& tx)
        -> std::optional<tx_error>;
    auto check_output_count(const transaction::full_tx& tx)
        -> std::optional<tx_error>;
    auto check_output_rangeproofs_exist(const transaction::full_tx& tx)
        -> std::optional<proof_error>;
    auto check_witness_count(const transaction::full_tx& tx)
        -> std::optional<tx_error>;
    auto check_input_set(const transaction::full_tx& tx)
        -> std::optional<tx_error>;
    auto check_range(const commitment_t& comm, const rangeproof_t& rng)
        -> std::optional<proof_error>;
    auto range_batch_add(secp256k1_ecmult_multi_batch& batch,
                         secp256k1_scratch_space* scratch,
                         const rangeproof_t& rng,
                         secp256k1_pedersen_commitment& comm)
        -> std::optional<proof_error>;
    auto check_range_batch(secp256k1_ecmult_multi_batch& batch)
        -> std::optional<proof_error>;
    auto check_proof(const compact_tx& tx,
                     const std::vector<commitment_t>& inps)
        -> std::optional<proof_error>;
    auto check_commitment_sum(
        const std::vector<secp256k1_pedersen_commitment>& inputs,
        const std::vector<secp256k1_pedersen_commitment>& outputs,
        uint64_t minted) -> bool;
    auto get_p2pk_witness_commitment(const pubkey_t& payee) -> hash_t;
    auto to_string(const tx_error& err) -> std::string;

    /// Validates the sentinel attestations attached to a compact transaction.
    /// \param tx compact transaction to validate.
    /// \param pubkeys set of public keys whose attestations will be accepted.
    /// \param threshold number of attestations required for a transaction to
    ///                  be considered valid.
    /// \return true if the required number of unique attestations are attached
    ///         to the compact transaction.
    auto check_attestations(
        const transaction::compact_tx& tx,
        const std::unordered_set<pubkey_t, hashing::null>& pubkeys,
        size_t threshold) -> bool;
}

#endif // OPENCBDC_TX_SRC_TRANSACTION_VALIDATION_H_
