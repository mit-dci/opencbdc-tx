// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_AGENT_RUNNERS_EVM_SERIALIZATION_H_
#define OPENCBDC_TX_SRC_3PC_AGENT_RUNNERS_EVM_SERIALIZATION_H_

#include "messages.hpp"
#include "signature.hpp"
#include "util/common/buffer.hpp"
#include "util/common/hash.hpp"
#include "util/common/keys.hpp"
#include "util/common/logging.hpp"

#include <evmc/evmc.hpp>
#include <evmc/hex.hpp>
#include <json/json.h>
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
    /// \return the evm_tx that was decoded or std::nullopt if no valid
    ///         value could be decoded
    auto tx_decode(const cbdc::buffer& buf,
                   const std::shared_ptr<logging::log>& logger,
                   uint64_t chain_id = opencbdc_chain_id)
        -> std::optional<
            std::shared_ptr<cbdc::threepc::agent::runner::evm_tx>>;

    /// Converts a given Json::Value to an evm_tx
    /// \param param Json::Value containing the raw transaction to decode
    ///             in (0x prefixed) hexadecimal format
    /// \return the evm_tx that was decoded or std::nullopt if no valid
    ///         value could be decoded
    auto raw_tx_from_json(const Json::Value& param) -> std::optional<
        std::shared_ptr<cbdc::threepc::agent::runner::evm_tx>>;

    /// Converts a given Json::Value to an evm_tx
    /// \param json Json::Value containing the transaction to decode
    /// \param chain_id the expected chain ID for the transaction. If the
    // transaction contains a different chain ID this method will return
    // std::nullopt
    /// \return the evm_tx that was decoded or std::nullopt if no valid
    ///         value could be decoded
    auto tx_from_json(const Json::Value& json,
                      uint64_t chain_id = opencbdc_chain_id)
        -> std::optional<
            std::shared_ptr<cbdc::threepc::agent::runner::evm_tx>>;

    /// Converts a given Json::Value to an evm_dryrun_tx
    /// \param json Json::Value containing the transaction to decode
    /// \param chain_id the expected chain ID for the transaction. If the
    // transaction contains a different chain ID this method will return
    // std::nullopt
    /// \return the evm_dryrun_tx that was decoded or std::nullopt if no valid
    ///         value could be decoded
    auto dryrun_tx_from_json(const Json::Value& json,
                             uint64_t chain_id = opencbdc_chain_id)
        -> std::optional<
            std::shared_ptr<cbdc::threepc::agent::runner::evm_dryrun_tx>>;

    /// Converts a given Json::Value to an evmc::address
    /// \param addr Json::Value containing the address to decode. Is expected
    ///             to be a string in 0x... format
    /// \return the evmc::address that was decoded or std::nullopt if no valid
    ///         value could be decoded
    auto address_from_json(const Json::Value& addr)
        -> std::optional<evmc::address>;

    /// Converts a given Json::Value to an evmc::uint256be
    /// \param val Json::Value containing the uint256be to decode. Is expected
    ///             to be a string in 0x... format
    /// \return the evmc::uint256be that was decoded or std::nullopt if no
    ///         valid value could be decoded
    auto uint256be_from_json(const Json::Value& val)
        -> std::optional<evmc::uint256be>;

    /// Converts a given Json::Value to a cbdc::buffer
    /// \param val Json::Value containing the buffer to decode. Is expected
    ///            to be a string in 0x... format containing valid hexadecimal
    ///            representation of the buffer
    /// \return the cbdc::buffer that was decoded or std::nullopt if no valid
    ///         value could be decoded
    auto buffer_from_json(const Json::Value& val)
        -> std::optional<cbdc::buffer>;

    /// Converts a given Json::Value to an evmc::uint256be, returning a default
    /// value if none could be decoded
    /// \param val Json::Value containing the buffer to decode. Is expected
    ///            to be a string in 0x... format containing valid hexadecimal
    ///            representation of the buffer
    /// \param def The value to return if no valid value could be decoded from
    ///            val
    /// \return the evmc::uint256be that was decoded or the value of def if no
    ///         value could be decoded
    auto uint256be_or_default(const Json::Value& val, evmc::uint256be def)
        -> evmc::uint256be;

    /// Encodes the given transaction into a eth-RPC compatible representation
    /// in JSON - as Json::Value
    /// \param tx  The transaction to represent as json
    /// \param ctx The secp256k1 context to use for deriving the from address
    /// \return a Json::Value containing the json representation of the
    ///         transaction
    auto tx_to_json(cbdc::threepc::agent::runner::evm_tx& tx,
                    const std::shared_ptr<secp256k1_context>& ctx)
        -> Json::Value;

    /// Encodes the given transaction receipt into a eth-RPC compatible
    /// representation in JSON - as Json::Value
    /// \param rcpt  The transaction receipt to represent as json
    /// \param ctx The secp256k1 context to use for deriving the from address
    /// \return a Json::Value containing the json representation of the
    ///         transaction receipt
    auto tx_receipt_to_json(cbdc::threepc::agent::runner::evm_tx_receipt& rcpt,
                            const std::shared_ptr<secp256k1_context>& ctx)
        -> Json::Value;

    /// Encodes the given transaction log into a eth-RPC compatible
    /// representation in JSON - as Json::Value
    /// \param log   The transaction log to represent as json
    /// \param tn    The ticket number this log was emitted by. Used to
    ///              generate a pretend blockhash. The evm_log does
    ///              not contain this data - so it has to be passed in.
    /// \param txid  The transaction ID this log was emitted by. The evm_log
    ///              does not contain this data - so it has to be passed in.
    /// \return a Json::Value containing the json representation of the
    ///         transaction log
    auto tx_log_to_json(cbdc::threepc::agent::runner::evm_log& log,
                        interface::ticket_number_type tn,
                        cbdc::hash_t txid) -> Json::Value;

    /// Encodes the given access list into a eth-RPC compatible representation
    /// in JSON - as Json::Value
    /// \param al  The access list to represent as json
    /// \return a Json::Value containing the json representation of the access
    ///         list
    auto access_list_to_json(cbdc::threepc::agent::runner::evm_access_list& al)
        -> Json::Value;

    /// Calculate ethereum-compatible txid
    /// \param tx transaction to calculate ID for
    /// \param chain_id unique chain ID, defaults to 0xcbdc.
    /// \return the eth compatible txid of the transaction
    auto tx_id(const cbdc::threepc::agent::runner::evm_tx& tx,
               uint64_t chain_id = opencbdc_chain_id) -> cbdc::hash_t;
}

#endif
