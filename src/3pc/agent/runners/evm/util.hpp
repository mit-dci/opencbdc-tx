// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CBDC_UNIVERSE0_SRC_3PC_AGENT_RUNNERS_EVM_UTIL_H_
#define CBDC_UNIVERSE0_SRC_3PC_AGENT_RUNNERS_EVM_UTIL_H_

#include "3pc/broker/interface.hpp"
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
    /// Converts an uint256be to a uint64_t, ignoring higher order bits.
    /// \param v bignum to convert.
    /// \return converted bignum.
    auto to_uint64(const evmc::uint256be& v) -> uint64_t;

    /// Converts a bytes-like object to a hex string.
    /// \tparam T type to convert from.
    /// \param v value to convert.
    /// \return hex string representation of v.
    template<typename T>
    auto to_hex(const T& v) -> std::string {
        return evmc::hex(evmc::bytes(v.bytes, sizeof(v.bytes)));
    }

    auto to_hex_trimmed(const evmc::bytes32& b,
                        const std::string& prefix = "0x") -> std::string;

    /// Adds an entry to a bloom value
    /// \param bloom the existing bloom value
    /// \param entry the entry to add
    /// \see https://ethereum.github.io/execution-specs/autoapi/ethereum/
    ///      paris/bloom/index.html
    void add_to_bloom(cbdc::buffer& bloom, const cbdc::buffer& entry);

    /// Parses hexadecimal representation in string format to T
    /// \tparam T type to convert from hex to.
    /// \param hex hex string to parse. May be prefixed with 0x
    /// \return object containing the parsed T or std::nullopt if
    /// parse failed
    template<typename T>
    auto from_hex(const std::string& hex) ->
        typename std::enable_if_t<std::is_same<T, evmc::bytes32>::value
                                      || std::is_same<T, evmc::address>::value,
                                  std::optional<T>> {
        auto maybe_bytes = cbdc::buffer::from_hex_prefixed(hex);
        if(!maybe_bytes.has_value()) {
            return std::nullopt;
        }
        if(maybe_bytes.value().size() != sizeof(T)) {
            return std::nullopt;
        }

        auto val = T();
        std::memcpy(val.bytes,
                    maybe_bytes.value().data(),
                    maybe_bytes.value().size());
        return val;
    }

    /// Generates a uint256be from a hex string.
    /// \param hex string to parse.
    /// \return uint256be from string, or std::nullopt if input is not a valid
    ///         hex string.
    auto uint256be_from_hex(const std::string& hex)
        -> std::optional<evmc::uint256be>;

    /// Mints a set of initial accounts with funds, bypassing the agent.
    /// \param log logger instance.
    /// \param broker broker instance to mint with.
    auto mint_initial_accounts(
        const std::shared_ptr<logging::log>& log,
        const std::shared_ptr<threepc::broker::interface>& broker) -> bool;
}

#endif
