// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_AGENT_RUNNERS_EVM_ADDRESS_H_
#define OPENCBDC_TX_SRC_3PC_AGENT_RUNNERS_EVM_ADDRESS_H_

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
    /// Calculates a contract address for the CREATE call
    /// keccak256(rlp([sender,nonce]))
    /// \param sender the sender account creating the contract
    /// \param nonce the account nonce of the sender at the time of creation
    /// \return the contract address
    auto contract_address(const evmc::address& sender,
                          const evmc::uint256be& nonce) -> evmc::address;

    /// Calculates a contract address for the CREATE2 call
    /// keccak256(0xFF | sender | salt | keccak256(bytecode))
    /// \param sender the sender account creating the contract
    /// \param salt the salt value
    /// \param bytecode_hash the keccak256 hash of the bytecode of the contract
    /// \return the contract address
    auto contract_address2(const evmc::address& sender,
                           const evmc::bytes32& salt,
                           const cbdc::hash_t& bytecode_hash) -> evmc::address;

    /// Calculates an eth address from a private key
    /// \param key key to calculate the address for
    /// \param ctx secp256k1 context to use
    /// \return the address corresponding to the passed private key
    auto eth_addr(const cbdc::privkey_t& key,
                  const std::shared_ptr<secp256k1_context>& ctx)
        -> evmc::address;

    /// Calculates an eth address from a public key
    /// \param pk key to calculate the address for
    /// \param ctx secp256k1 context to use
    /// \return the address corresponding to the passed public key
    auto eth_addr(const std::unique_ptr<secp256k1_pubkey>& pk,
                  const std::shared_ptr<secp256k1_context>& ctx)
        -> evmc::address;
}

#endif
