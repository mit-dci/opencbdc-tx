// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.hpp"

#include "uhs/transaction/messages.hpp"
#include "util/common/hash.hpp"
#include "util/common/hashmap.hpp"

namespace cbdc::test {
    auto compact_transaction::operator==(
        const transaction::compact_tx& tx) const noexcept -> bool {
        return m_id == tx.m_id && (m_inputs == tx.m_inputs)
            && (m_uhs_outputs == tx.m_uhs_outputs);
    }

    compact_transaction::compact_transaction(
        const transaction::compact_tx& tx) {
        m_id = tx.m_id;
        m_uhs_outputs = tx.m_uhs_outputs;
        m_inputs = tx.m_inputs;
    }

    auto compact_transaction_hasher::operator()(
        const compact_transaction& tx) const noexcept -> size_t {
        auto buf = cbdc::make_buffer(tx);
        return std::hash<std::string>()(buf.to_hex());
    }

    auto block::operator==(const cbdc::atomizer::block& rhs) const noexcept
        -> bool {
        return (rhs.m_height == m_height)
            && (std::vector<compact_transaction>(rhs.m_transactions.begin(),
                                                 rhs.m_transactions.end())
                == std::vector<compact_transaction>(m_transactions.begin(),
                                                    m_transactions.end()));
    }

    auto simple_tx(const hash_t& id,
                   const std::vector<transaction::uhs_element>& ins,
                   const std::vector<transaction::uhs_element>& outs)
        -> compact_transaction {
        compact_transaction tx{};
        tx.m_id = id;
        tx.m_inputs = ins;
        tx.m_uhs_outputs = outs;
        return tx;
    }

    void print_sentinel_error(
        const std::optional<transaction::validation::tx_error>& err) {
        if(err.has_value()) {
            std::cout << transaction::validation::to_string(err.value())
                      << std::endl;
        }
    }

    void load_config(const std::string& config_file,
                     cbdc::config::options& opts) {
        auto opts_or_err = cbdc::config::load_options(config_file);
        ASSERT_TRUE(
            std::holds_alternative<cbdc::config::options>(opts_or_err));
        opts = std::get<cbdc::config::options>(opts_or_err);
    }

    void sign_tx(compact_transaction& tx, const privkey_t& key) {
        auto secp = std::unique_ptr<secp256k1_context,
                                    decltype(&secp256k1_context_destroy)>{
            secp256k1_context_create(SECP256K1_CONTEXT_SIGN),
            &secp256k1_context_destroy};
        auto att = tx.sign(secp.get(), key);
        tx.m_attestations.insert(att);
    }
}
