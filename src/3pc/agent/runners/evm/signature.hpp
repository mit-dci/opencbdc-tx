// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_AGENT_RUNNERS_EVM_SIGNATURE_H_
#define OPENCBDC_TX_SRC_3PC_AGENT_RUNNERS_EVM_SIGNATURE_H_

#include "messages.hpp"
#include "util/common/buffer.hpp"
#include "util/common/hash.hpp"
#include "util/common/keys.hpp"

#include <evmc/evmc.hpp>
#include <evmc/hex.hpp>
#include <memory>
#include <secp256k1.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_recovery.h>

namespace cbdc::threepc::agent::runner {
    /// Signs a hash using a privkey_t using ecdsa and produces an evm_sig
    /// struct Used primarily in unit tests for signature checking
    /// \param key key to sign with
    /// \param hash hash to sign
    /// \param type the transaction type
    /// \param ctx secp256k1 context to use
    /// \param chain_id the chain_id we are signing for (defaults to opencbdc)
    /// \return the signature value encoded in r,s,v values in an evm_sig struct
    auto eth_sign(const privkey_t& key,
                  hash_t& hash,
                  evm_tx_type type,
                  const std::shared_ptr<secp256k1_context>& ctx,
                  uint64_t chain_id = opencbdc_chain_id) -> evm_sig;

    /// Checks the signature of an EVM transaction
    /// \param tx transaction to check signature for
    /// \param chain_id chain_id for which the transaction is meant
    /// \param ctx secp256k1 context to use
    /// \return the signer's address if the signature is valid and recoverable,
    /// nullopt otherwise
    auto check_signature(const cbdc::threepc::agent::runner::evm_tx& tx,
                         const std::shared_ptr<secp256k1_context>& ctx,
                         uint64_t chain_id = opencbdc_chain_id)
        -> std::optional<evmc::address>;

    /// Calculates the hash for creating / validating the signature
    /// \param tx transaction to calculate the sighash for
    /// \param chain_id unique chain ID, defaults to 0xcbdc.
    /// \return the sighash of the transaction
    auto sig_hash(const cbdc::threepc::agent::runner::evm_tx& tx,
                  uint64_t chain_id = opencbdc_chain_id) -> hash_t;

}
#endif
