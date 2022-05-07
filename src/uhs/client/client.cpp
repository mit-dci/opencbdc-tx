// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "client.hpp"

#include "bech32/bech32.h"
#include "bech32/util/strencodings.h"
#include "crypto/sha256.h"
#include "uhs/sentinel/format.hpp"
#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/wallet.hpp"
#include "util/common/config.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/istream_serializer.hpp"
#include "util/serialization/ostream_serializer.hpp"
#include "util/serialization/util.hpp"

#include <filesystem>
#include <iomanip>
#include <utility>

namespace cbdc {

    namespace address {
        auto decode(const std::string& addr_str)
            -> std::optional<cbdc::hash_t> {
            // TODO: if/when bech32m is merged into Bitcoin Core, switch to
            // that.
            //       see: https://github.com/bitcoin/bitcoin/pull/20861
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
            if(data[0]
                   != static_cast<uint8_t>(
                       cbdc::client::address_type::public_key)
               || data.size() != pubkey.size() + 1) {
                std::cout << "Address is not a supported type" << std::endl;
                return std::nullopt;
            }

            data.erase(data.begin());
            std::copy_n(data.begin(), pubkey.size(), pubkey.begin());

            return pubkey;
        }
    }

    client::client(cbdc::config::options opts,
                   std::shared_ptr<logging::log> logger,
                   std::string wallet_file,
                   std::string client_file)
        : m_opts(std::move(opts)),
          m_logger(std::move(logger)),
          m_sentinel_client(m_opts.m_sentinel_endpoints, m_logger),
          m_client_file(std::move(client_file)),
          m_wallet_file(std::move(wallet_file)) {}

    auto client::init() -> bool {
        if(std::filesystem::exists(m_wallet_file)) {
            m_wallet.load(m_wallet_file);
        } else {
            m_logger->warn("Existing wallet file not found");
        }

        load_client_state();

        if(!m_sentinel_client.init()) {
            m_logger->error("Failed to initialize sentinel client.");
            return false;
        }

        return init_derived();
    }

    auto client::print_amount(uint64_t val) -> std::string {
        std::stringstream ss;
        ss << config::currency_symbol << std::fixed << std::setprecision(2)
           << static_cast<double>(val) / 100.0;
        return ss.str();
    }

    auto client::mint(size_t n_outputs, uint32_t output_val)
        -> transaction::full_tx {
        auto mint_tx = m_wallet.mint_new_coins(n_outputs, output_val);
        import_transaction(mint_tx);

        // TODO: make a formal way of minting. For now bypass the sentinels.
        if(!send_mint_tx(mint_tx)) {
            m_logger->error("Failed to send mint tx");
        }

        return mint_tx;
    }

    void client::sign_transaction(transaction::full_tx& tx) {
        auto keys = m_wallet.spending_keys(tx);
        assert(keys.has_value());
        m_wallet.sign(tx, keys.value());
    }

    void client::register_pending_tx(const transaction::full_tx& tx) {
        // Mark all inputs as pending spend
        for(const auto& in : tx.m_inputs) {
            m_pending_spend.insert({in.hash(), in});
        }

        save();
    }

    auto client::create_transaction(uint32_t value, const pubkey_t& payee)
        -> std::optional<transaction::full_tx> {
        auto tx = m_wallet.send_to(value, payee, true);
        if(!tx.has_value()) {
            return std::nullopt;
        }

        register_pending_tx(tx.value());

        return tx;
    }

    auto client::send(uint32_t value, const pubkey_t& payee)
        -> std::pair<std::optional<transaction::full_tx>,
                     std::optional<cbdc::sentinel::execute_response>> {
        static constexpr auto null_return
            = std::make_pair(std::nullopt, std::nullopt);

        auto spend_tx = create_transaction(value, payee);
        if(!spend_tx.has_value()) {
            m_logger->error("Failed to generate wallet spend tx.");
            return null_return;
        }

        auto res = send_transaction(spend_tx.value());
        if(!res.has_value()) {
            return null_return;
        }

        return std::make_pair(spend_tx.value(), res.value());
    }

    auto client::fan(uint32_t count, uint32_t value, const pubkey_t& payee)
        -> std::pair<std::optional<transaction::full_tx>,
                     std::optional<cbdc::sentinel::execute_response>> {
        static constexpr auto null_return
            = std::make_pair(std::nullopt, std::nullopt);

        auto tx = m_wallet.fan(count, value, payee, true);
        if(!tx.has_value()) {
            m_logger->error("Failed to generate wallet fan tx");
            return null_return;
        }

        register_pending_tx(tx.value());

        auto res = send_transaction(tx.value());
        if(!res.has_value()) {
            return null_return;
        }

        return std::make_pair(tx.value(), res.value());
    }

    auto client::send_transaction(const transaction::full_tx& tx)
        -> std::optional<cbdc::sentinel::execute_response> {
        import_transaction(tx);

        auto res = m_sentinel_client.execute_transaction(tx);
        if(!res.has_value()) {
            m_logger->error("Failed to send transaction to sentinel.");
            return std::nullopt;
        }

        m_logger->info("Sentinel responded:",
                       cbdc::sentinel::to_string(res.value().m_tx_status),
                       "for",
                       to_string(transaction::tx_id(tx)));

        if(res.value().m_tx_status == sentinel::tx_status::confirmed) {
            confirm_transaction(transaction::tx_id(tx));
        }

        return res;
    }

    auto client::export_send_inputs(const transaction::full_tx& send_tx,
                                    const pubkey_t& payee)
        -> std::vector<transaction::input> {
        return transaction::wallet::export_send_inputs(send_tx, payee);
    }

    void client::import_send_input(const transaction::input& in) {
        if(m_wallet.is_spendable(in)) {
            m_pending_inputs.insert({in.m_prevout.m_tx_id, in});
            save();
        } else {
            m_logger->warn("Ignoring non-spendable input");
        }
    }

    auto client::new_address() -> pubkey_t {
        auto addr = m_wallet.generate_key();
        save();
        return addr;
    }

    auto client::balance() -> uint64_t {
        return m_wallet.balance();
    }

    auto client::utxo_count() -> size_t {
        return m_wallet.count();
    }

    auto client::pending_tx_count() -> size_t {
        return m_pending_txs.size();
    }

    auto client::pending_input_count() -> size_t {
        return m_pending_inputs.size();
    }

    void client::import_transaction(const transaction::full_tx& tx) {
        m_pending_txs.insert({transaction::tx_id(tx), tx});
        save();
    }

    auto client::check_pending(const transaction::input& inp) -> bool {
        // TODO: This can probably be done more efficiently, but we
        // might have to keep a secondary map to index the pending tx set on
        // input hash?
        for(auto& it : m_pending_txs) {
            for(auto& in : it.second.m_inputs) {
                if(in == inp) {
                    return true;
                }
            }
        }
        return false;
    }

    auto client::abandon_transaction(const hash_t& tx_id) -> bool {
        bool success{false};
        const auto it = m_pending_txs.find(tx_id);
        if(it != m_pending_txs.end()) {
            auto tx = it->second;
            m_pending_txs.erase(it);

            // Add the used inputs back to the wallet if they are still
            // pending and not used in any other pending transaction.
            for(auto& i : tx.m_inputs) {
                const auto ps_it = m_pending_spend.find(i.hash());
                if(ps_it != m_pending_spend.end()) {
                    auto pending = check_pending(i);
                    if(!pending) {
                        m_pending_spend.erase(ps_it);
                        m_wallet.confirm_inputs({i});
                    }
                }
            }
            success = true;
        }

        save();

        return success;
    }

    auto client::confirm_transaction(const hash_t& tx_id) -> bool {
        // TODO: Should abandon and confirm be combined somehow?
        // for instance: finish_transaction(hash_t tx_id, bool succeeded)
        bool success{false};
        const auto it = m_pending_txs.find(tx_id);
        if(it != m_pending_txs.end()) {
            m_wallet.confirm_transaction(it->second);
            for(auto& i : it->second.m_inputs) {
                const auto ps_it = m_pending_spend.find(i.hash());
                if(ps_it != m_pending_spend.end()) {
                    m_pending_spend.erase(ps_it);
                }
            }
            m_pending_txs.erase(it);
            success = true;
        }

        const auto pi_it = m_pending_inputs.find(tx_id);
        if(pi_it != m_pending_inputs.end()) {
            m_wallet.confirm_inputs({pi_it->second});
            m_pending_inputs.erase(pi_it);
            success = true;
        }

        save();

        return success;
    }

    void client::load_client_state() {
        std::ifstream client_file(m_client_file,
                                  std::ios::binary | std::ios::in);
        if(client_file.good()) {
            auto deser = cbdc::istream_serializer(client_file);
            if(!(deser >> m_pending_txs >> m_pending_inputs
                 >> m_pending_spend)) {
                m_logger->fatal("Error deserializing client file");
            }
        } else {
            m_logger->warn("Existing client file not found");
        }
    }

    void client::save_client_state() {
        std::ofstream client_file(m_client_file,
                                  std::ios::binary | std::ios::trunc
                                      | std::ios::out);
        if(!client_file.good()) {
            m_logger->fatal("Failed to open client file for saving");
        }
        auto ser = cbdc::ostream_serializer(client_file);
        if(!(ser << m_pending_txs << m_pending_inputs << m_pending_spend)) {
            m_logger->fatal("Failed to write client data");
        }
    }

    void client::save() {
        save_client_state();
        m_wallet.save(m_wallet_file);
    }

    auto client::pending_txs() const
        -> std::unordered_map<hash_t, transaction::full_tx, hashing::null> {
        return m_pending_txs;
    }

    auto client::pending_inputs() const
        -> std::unordered_map<hash_t, transaction::input, hashing::null> {
        return m_pending_inputs;
    }
}
