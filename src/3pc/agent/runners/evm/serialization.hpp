// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CBDC_UNIVERSE0_SRC_3PC_AGENT_RUNNERS_EVM_SERIALIZATION_H_
#define CBDC_UNIVERSE0_SRC_3PC_AGENT_RUNNERS_EVM_SERIALIZATION_H_

#include "messages.hpp"
#include "util/common/buffer.hpp"
#include "util/common/hash.hpp"
#include "util/common/keys.hpp"
#include "util/common/logging.hpp"

#include <evmc/evmc.hpp>
#include <evmc/hex.hpp>
#include <memory>
#include <secp256k1.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_recovery.h>

namespace cbdc::threepc::agent::runner {
    static constexpr uint64_t eip155_v_offset = 35;
    static constexpr uint64_t pre_eip155_v_offset = 27;

    /// Converts the given transaction to an RLP encoded buffer conforming to
    /// Ethereums conventions
    /// \param tx transaction to encode
    /// \param chain_id the chain ID for which to encode the transaction
    /// \param for_sighash use the formatting needed to calculate the sighash
    /// \return the rlp representation of the transaction
    auto tx_encode(const cbdc::threepc::agent::runner::evm_tx& tx,
                   uint64_t chain_id = opencbdc_chain_id,
                   bool for_sighash = false) -> cbdc::buffer;

    /// Converts a given buffer to an evm_tx
    /// \param buf buffer containing the transaction to decode
    /// \param logger logger to output any parsing errors to
    /// \param chain_id the expected chain ID for the transaction. If the
    // transaction contains a different chain ID this method will return
    // std::nullopt
    /// \return the evm_tx that was decoded
    auto tx_decode(const cbdc::buffer& buf,
                   const std::shared_ptr<logging::log>& logger,
                   uint64_t chain_id = opencbdc_chain_id)
        -> std::optional<
            std::shared_ptr<cbdc::threepc::agent::runner::evm_tx>>;

    /// Calculate ethereum-compatible txid
    /// \param tx transaction to calculate ID for
    /// \param chain_id unique chain ID, defaults to 0xcbdc.
    /// \return the eth compatible txid of the transaction
    auto tx_id(const cbdc::threepc::agent::runner::evm_tx& tx,
               uint64_t chain_id = opencbdc_chain_id) -> cbdc::hash_t;
}

#endif
