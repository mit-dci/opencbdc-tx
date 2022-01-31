// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "atomizer_client.hpp"
#include "bech32/bech32.h"
#include "bech32/util/strencodings.h"
#include "client.hpp"
#include "crypto/sha256.h"
#include "twophase_client.hpp"
#include "uhs/transaction/messages.hpp"
#include "util/common/config.hpp"
#include "util/serialization/util.hpp"

#include <future>
#include <iostream>

static constexpr auto bits_per_byte = 8;
static constexpr auto bech32_bits_per_symbol = 5;

auto mint_command(cbdc::client& client, const std::vector<std::string>& args)
    -> bool {
    static constexpr auto min_mint_arg_count = 7;
    if(args.size() < min_mint_arg_count) {
        std::cerr << "Mint requires args <n outputs> <output value>"
                  << std::endl;
        return false;
    }

    const auto n_outputs = std::stoull(args[5]);
    const auto output_val = std::stoul(args[6]);

    const auto mint_tx
        = client.mint(n_outputs, static_cast<uint32_t>(output_val));
    std::cout << cbdc::to_string(cbdc::transaction::tx_id(mint_tx))
              << std::endl;
    return true;
}

auto decode_address(const std::string& addr_str)
    -> std::optional<cbdc::hash_t> {
    // TODO: if/when bech32m is merged into Bitcoin Core, switch to that.
    //       see: https://github.com/bitcoin/bitcoin/pull/20861
    // TODO: move address encoding/decoding into cbdc::client.
    const auto [hrp, enc_data] = bech32::Decode(addr_str);
    if(hrp != cbdc::config::bech32_hrp) {
        std::cout << "Invalid address encoding" << std::endl;
        return std::nullopt;
    }
    auto data = std::vector<uint8_t>();
    ConvertBits<bech32_bits_per_symbol, bits_per_byte, false>(
        [&](uint8_t c) {
            data.push_back(c);
        },
        enc_data.begin(),
        enc_data.end());

    auto pubkey = cbdc::hash_t();
    if(data[0] != static_cast<uint8_t>(cbdc::client::address_type::public_key)
       || data.size() != pubkey.size() + 1) {
        std::cout << "Address is not a supported type" << std::endl;
        return std::nullopt;
    }
    data.erase(data.begin());
    std::copy_n(data.begin(), pubkey.size(), pubkey.begin());

    return pubkey;
}

void print_tx_result(const std::optional<cbdc::transaction::full_tx>& tx,
                     const std::optional<cbdc::sentinel::response>& resp,
                     const cbdc::hash_t& pubkey) {
    std::cout << "tx_id:" << std::endl
              << cbdc::to_string(cbdc::transaction::tx_id(tx.value()))
              << std::endl;
    const auto inputs = cbdc::client::export_send_inputs(tx.value(), pubkey);
    for(const auto& inp : inputs) {
        auto buf = cbdc::make_buffer(inp);
        std::cout << "Data for recipient importinput:" << std::endl
                  << buf.to_hex() << std::endl;
    }

    if(resp.has_value()) {
        std::cout << "Sentinel responded: "
                  << cbdc::sentinel::to_string(resp.value().m_tx_status)
                  << std::endl;
        if(resp.value().m_tx_error.has_value()) {
            std::cout << "Validation error: "
                      << cbdc::transaction::validation::to_string(
                             resp.value().m_tx_error.value())
                      << std::endl;
        }
    }
}

auto send_command(cbdc::client& client, const std::vector<std::string>& args)
    -> bool {
    static constexpr auto min_send_arg_count = 7;
    if(args.size() < min_send_arg_count) {
        std::cerr << "Send requires args <value> <pubkey>" << std::endl;
        return false;
    }

    const auto value = std::stoul(args[5]);
    static constexpr auto address_arg_idx = 6;
    auto pubkey = decode_address(args[address_arg_idx]);
    if(!pubkey.has_value()) {
        std::cout << "Could not decode address" << std::endl;
        return false;
    }

    const auto [tx, resp]
        = client.send(static_cast<uint32_t>(value), pubkey.value());
    if(!tx.has_value()) {
        std::cout << "Could not generate valid send tx." << std::endl;
        return false;
    }

    print_tx_result(tx, resp, pubkey.value());
    return true;
}

auto fan_command(cbdc::client& client, const std::vector<std::string>& args)
    -> bool {
    static constexpr auto min_fan_arg_count = 8;
    if(args.size() < min_fan_arg_count) {
        std::cerr << "Fan requires args <count> <value> <pubkey>" << std::endl;
        return false;
    }

    const auto value = std::stoul(args[6]);
    const auto count = std::stoul(args[5]);

    static constexpr auto address_arg_idx = 7;
    auto pubkey = decode_address(args[address_arg_idx]);
    if(!pubkey.has_value()) {
        std::cout << "Could not decode address" << std::endl;
        return false;
    }

    const auto [tx, resp] = client.fan(static_cast<uint32_t>(count),
                                       static_cast<uint32_t>(value),
                                       pubkey.value());
    if(!tx.has_value()) {
        std::cout << "Could not generate valid send tx." << std::endl;
        return false;
    }

    print_tx_result(tx, resp, pubkey.value());
    return true;
}

void newaddress_command(cbdc::client& client) {
    const auto addr = client.new_address();
    auto addr_vec
        = std::vector<uint8_t>(sizeof(cbdc::client::address_type::public_key)
                               + std::tuple_size<decltype(addr)>::value);
    addr_vec[0] = static_cast<uint8_t>(cbdc::client::address_type::public_key);
    std::copy_n(addr.begin(),
                addr.size(),
                addr_vec.begin()
                    + sizeof(cbdc::client::address_type::public_key));
    auto data = std::vector<uint8_t>();
    ConvertBits<bits_per_byte, bech32_bits_per_symbol, true>(
        [&](uint8_t c) {
            data.push_back(c);
        },
        addr_vec.begin(),
        addr_vec.end());
    std::cout << bech32::Encode(cbdc::config::bech32_hrp, data) << std::endl;
}

auto importinput_command(cbdc::client& client,
                         const std::vector<std::string>& args) -> bool {
    static constexpr auto input_arg_idx = 5;
    auto buffer = cbdc::buffer::from_hex(args[input_arg_idx]);
    if(!buffer.has_value()) {
        std::cout << "Invalid input encoding." << std::endl;
        return false;
    }

    auto in = cbdc::from_buffer<cbdc::transaction::input>(buffer.value());
    if(!in.has_value()) {
        std::cout << "Invalid input" << std::endl;
        return false;
    }
    client.import_send_input(in.value());
    return true;
}

auto confirmtx_command(cbdc::client& client,
                       const std::vector<std::string>& args) -> bool {
    const auto tx_id = cbdc::hash_from_hex(args[5]);
    auto success = client.confirm_transaction(tx_id);
    if(!success) {
        std::cout << "Unknown TXID" << std::endl;
        return false;
    }
    std::cout << "Confirmed. Balance: "
              << cbdc::client::print_amount(client.balance())
              << " UTXOs: " << client.utxo_count() << std::endl;
    return true;
}

// LCOV_EXCL_START
auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    static constexpr auto min_arg_count = 5;
    if(args.size() < min_arg_count) {
        std::cerr << "Usage: " << args[0]
                  << " <config file> <client file> <wallet file> <command>"
                  << " <args...>" << std::endl;
        return 0;
    }

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        std::cerr << "Error loading config file: "
                  << std::get<std::string>(cfg_or_err) << std::endl;
        return -1;
    }

    auto opts = std::get<cbdc::config::options>(cfg_or_err);

    SHA256AutoDetect();

    const auto wallet_file = args[3];
    const auto client_file = args[2];

    auto logger = std::make_shared<cbdc::logging::log>(
        cbdc::config::defaults::log_level);

    auto client = std::unique_ptr<cbdc::client>();
    if(opts.m_twophase_mode) {
        client = std::make_unique<cbdc::twophase_client>(opts,
                                                         logger,
                                                         wallet_file,
                                                         client_file);
    } else {
        client = std::make_unique<cbdc::atomizer_client>(opts,
                                                         logger,
                                                         wallet_file,
                                                         client_file);
    }

    if(!client->init()) {
        return -1;
    }

    const auto command = std::string(args[4]);
    if(command == "mint") {
        if(!mint_command(*client, args)) {
            return -1;
        }
    } else if(command == "send") {
        if(!send_command(*client, args)) {
            return -1;
        }
    } else if(command == "fan") {
        if(!fan_command(*client, args)) {
            return -1;
        }
    } else if(command == "sync") {
        client->sync();
    } else if(command == "newaddress") {
        newaddress_command(*client);
    } else if(command == "info") {
        const auto balance = client->balance();
        const auto n_txos = client->utxo_count();
        std::cout << "Balance: " << cbdc::client::print_amount(balance)
                  << ", UTXOs: " << n_txos
                  << ", pending TXs: " << client->pending_tx_count()
                  << std::endl;
    } else if(command == "importinput") {
        if(!importinput_command(*client, args)) {
            return -1;
        }
    } else if(command == "confirmtx") {
        if(!confirmtx_command(*client, args)) {
            return -1;
        }
    } else {
        std::cerr << "Unkown command" << std::endl;
    }

    // TODO: check that the send queue has drained before closing
    //       the network. For now, just sleep.
    static constexpr auto shutdown_delay = std::chrono::milliseconds(100);
    std::this_thread::sleep_for(shutdown_delay);

    return 0;
}
// LCOV_EXCL_STOP
