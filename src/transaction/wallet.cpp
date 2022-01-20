// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.hpp"

#include "serialization/format.hpp"
#include "serialization/istream_serializer.hpp"
#include "serialization/ostream_serializer.hpp"
#include "transaction/messages.hpp"
#include "transaction/validation.hpp"

#include <secp256k1_schnorrsig.h>

namespace cbdc {
    transaction::wallet::wallet() {
        auto seed = std::chrono::high_resolution_clock::now()
                        .time_since_epoch()
                        .count();
        seed %= std::numeric_limits<uint32_t>::max();
        m_shuffle.seed(static_cast<uint32_t>(seed));
    }

    auto transaction::wallet::mint_new_coins(const size_t n_outputs,
                                             const uint32_t output_val)
        -> transaction::full_tx {
        transaction::full_tx ret;

        for(size_t i = 0; i < n_outputs; i++) {
            transaction::output out;

            const auto pubkey = generate_key();

            out.m_witness_program_commitment
                = transaction::validation::get_p2pk_witness_commitment(pubkey);

            out.m_value = output_val;

            ret.m_outputs.push_back(out);
        }

        return ret;
    }

    auto transaction::wallet::send_to(const uint32_t amount,
                                      const pubkey_t& payee,
                                      bool sign_tx)
        -> std::optional<transaction::full_tx> {
        auto maybe_tx = accumulate_inputs(amount);
        if(!maybe_tx.has_value()) {
            return std::nullopt;
        }

        auto& ret = maybe_tx.value().first;
        auto total_amount = maybe_tx.value().second;

        transaction::output destination_out;
        destination_out.m_value = amount;

        destination_out.m_witness_program_commitment
            = transaction::validation::get_p2pk_witness_commitment(payee);
        ret.m_outputs.push_back(destination_out);

        if(total_amount > amount) {
            // Add the change output if we need to
            transaction::output change_out;
            change_out.m_value = total_amount - amount;
            const auto pubkey = generate_key();
            change_out.m_witness_program_commitment
                = transaction::validation::get_p2pk_witness_commitment(pubkey);
            ret.m_outputs.push_back(change_out);
        }

        if(sign_tx) {
            sign(ret);
        }

        return ret;
    }

    auto transaction::wallet::create_seeded_transaction(size_t seed_idx)
        -> std::optional<transaction::full_tx> {
        if(m_seed_from == m_seed_to) {
            return std::nullopt;
        }
        transaction::full_tx tx;
        tx.m_inputs.resize(1);
        tx.m_outputs.resize(1);
        tx.m_inputs[0].m_prevout.m_tx_id = {0};
        tx.m_inputs[0].m_prevout_data.m_value = m_seed_value;
        tx.m_inputs[0].m_prevout_data.m_witness_program_commitment = {0};
        tx.m_outputs[0].m_witness_program_commitment
            = m_seed_witness_commitment;
        tx.m_outputs[0].m_value = m_seed_value;
        tx.m_inputs[0].m_prevout.m_index = seed_idx;
        return tx;
    }

    auto transaction::wallet::create_seeded_input(size_t seed_idx)
        -> std::optional<transaction::input> {
        if(auto tx = create_seeded_transaction(seed_idx)) {
            const auto& tx_id = transaction::tx_id(tx.value());
            return transaction::input_from_output(tx.value(), 0, tx_id);
        }
        return std::nullopt;
    }

    auto transaction::wallet::export_send_inputs(
        const transaction::full_tx& send_tx,
        const pubkey_t& payee) -> std::vector<transaction::input> {
        auto wit_comm
            = transaction::validation::get_p2pk_witness_commitment(payee);
        auto ret = std::vector<input>();
        for(uint32_t i = 0; i < send_tx.m_outputs.size(); i++) {
            if(send_tx.m_outputs[i].m_witness_program_commitment == wit_comm) {
                ret.push_back(
                    transaction::input_from_output(send_tx, i).value());
            }
        }
        return ret;
    }

    auto transaction::wallet::generate_key() -> pubkey_t {
        // Unique lock on m_keys, m_keygen and m_keygen_dist
        {
            // TODO: add config parameter where 0 = never reuse.
            static constexpr size_t max_keys = 10000;
            std::shared_lock<std::shared_mutex> lg(m_keys_mut);
            if(m_keys.size() > max_keys) {
                std::uniform_int_distribution<size_t> keyshuffle_dist(
                    0,
                    m_keys.size() - 1);
                const auto index = keyshuffle_dist(m_shuffle);
                return m_pubkeys[index];
            }
        }

        std::uniform_int_distribution<unsigned char> keygen;

        privkey_t seckey;
        for(auto&& b : seckey) {
            b = keygen(*m_random_source);
        }
        pubkey_t ret = pubkey_from_privkey(seckey, m_secp.get());
        {
            std::unique_lock<std::shared_mutex> lg(m_keys_mut);
            m_pubkeys.push_back(ret);
            m_keys.insert({ret, seckey});
            m_witness_programs.insert(
                {transaction::validation::get_p2pk_witness_commitment(ret),
                 ret});
        }

        return ret;
    }

    void transaction::wallet::sign(transaction::full_tx& tx) const {
        // TODO: other sighash types besides SIGHASH_ALL?
        const auto sighash = transaction::tx_id(tx);
        tx.m_witness.resize(tx.m_inputs.size());

        for(size_t i = 0; i < tx.m_inputs.size(); i++) {
            const auto& wit_commit
                = tx.m_inputs[i].m_prevout_data.m_witness_program_commitment;

            privkey_t seckey{};
            pubkey_t pubkey{};
            bool key_ours = false;
            {
                std::shared_lock<std::shared_mutex> sl(m_keys_mut);
                const auto wit_prog = m_witness_programs.find(wit_commit);
                key_ours = wit_prog != m_witness_programs.end();
                if(key_ours) {
                    pubkey = wit_prog->second;
                    seckey = m_keys.at(pubkey);
                }
            }

            if(key_ours) {
                auto& sig = tx.m_witness[i];
                sig.resize(transaction::validation::p2pk_witness_len);
                sig[0] = std::byte(
                    transaction::validation::witness_program_type::p2pk);
                std::memcpy(
                    &sig[sizeof(
                        transaction::validation::witness_program_type)],
                    pubkey.data(),
                    pubkey.size());

                secp256k1_keypair keypair{};
                [[maybe_unused]] const auto ret
                    = secp256k1_keypair_create(m_secp.get(),
                                               &keypair,
                                               seckey.data());
                assert(ret == 1);

                std::array<unsigned char, sig_len> sig_arr{};
                [[maybe_unused]] const auto sign_ret
                    = secp256k1_schnorrsig_sign(m_secp.get(),
                                                sig_arr.data(),
                                                sighash.data(),
                                                &keypair,
                                                nullptr,
                                                nullptr);
                std::memcpy(
                    &sig[transaction::validation::p2pk_witness_prog_len],
                    sig_arr.data(),
                    sizeof(sig_arr));
                assert(sign_ret == 1);
            }
        }
    }

    void transaction::wallet::update_balance(
        const std::vector<transaction::input>& credits,
        const std::vector<transaction::input>& debits) {
        std::unique_lock<std::shared_mutex> lu(m_utxos_mut);
        for(const auto& inp : credits) {
            const auto added = m_utxos_set.insert(inp);
            if(added.second) {
                m_balance += inp.m_prevout_data.m_value;
                m_spend_queue.push_back(inp);
            }
        }

        for(const auto& inp : debits) {
            const auto erased = m_utxos_set.erase(inp) > 0;
            if(erased) {
                m_balance -= inp.m_prevout_data.m_value;
            }
        }
        assert(m_spend_queue.size() == m_utxos_set.size());
    }

    auto transaction::wallet::seed(const privkey_t& privkey,
                                   uint32_t value,
                                   size_t begin_seed,
                                   size_t end_seed) -> bool {
        if(end_seed <= begin_seed) {
            return false;
        }

        pubkey_t pubkey = pubkey_from_privkey(privkey, m_secp.get());
        auto witness_commitment
            = transaction::validation::get_p2pk_witness_commitment(pubkey);
        {
            std::unique_lock<std::shared_mutex> lg(m_keys_mut);
            if(!m_keys.empty()) {
                return false;
            }
            m_pubkeys.push_back(pubkey);
            m_keys.insert({pubkey, privkey});
            m_witness_programs.insert({witness_commitment, pubkey});
        }
        seed_readonly(witness_commitment, value, begin_seed, end_seed);
        return true;
    }

    void transaction::wallet::seed_readonly(const hash_t& witness_commitment,
                                            uint32_t value,
                                            size_t begin_seed,
                                            size_t end_seed) {
        m_seed_from = begin_seed;
        m_seed_to = end_seed;
        m_seed_value = value;
        m_seed_witness_commitment = witness_commitment;
    }

    void
    transaction::wallet::confirm_transaction(const transaction::full_tx& tx) {
        const auto tx_id = transaction::tx_id(tx);
        std::vector<transaction::input> new_utxos;
        {
            std::shared_lock<std::shared_mutex> sl(m_keys_mut);
            for(uint32_t i = 0; i < tx.m_outputs.size(); i++) {
                const auto& out = tx.m_outputs[i];
                if(m_witness_programs.find(out.m_witness_program_commitment)
                   != m_witness_programs.end()) {
                    new_utxos.push_back(
                        transaction::input_from_output(tx, i, tx_id).value());
                }
            }
        }
        update_balance(new_utxos, tx.m_inputs);
    }

    auto transaction::wallet::balance() const -> uint64_t {
        std::shared_lock<std::shared_mutex> lg(m_utxos_mut);
        // TODO: handle overflow
        auto balance = m_balance;
        if(m_seed_from != m_seed_to) {
            balance += (m_seed_to - m_seed_from) * m_seed_value;
        }
        return balance;
    }

    auto transaction::wallet::count() const -> size_t {
        std::shared_lock<std::shared_mutex> lg(m_utxos_mut);
        auto size = m_utxos_set.size();
        if(m_seed_from != m_seed_to) {
            size += (m_seed_to - m_seed_from);
        }
        return size;
    }

    void transaction::wallet::save(const std::string& wallet_file) const {
        std::ofstream wal_file(wallet_file,
                               std::ios::binary | std::ios::trunc
                                   | std::ios::out);
        if(!wal_file.good()) {
            // TODO: add a logger to wallet or give the save/load function
            // return
            ///      values
            std::exit(EXIT_FAILURE);
        }
        auto ser = ostream_serializer(wal_file);
        {
            std::shared_lock<std::shared_mutex> lk(m_keys_mut);
            ser << m_keys;
        }

        {
            std::shared_lock<std::shared_mutex> lu(m_utxos_mut);
            ser << m_utxos_set;
        }
    }

    void transaction::wallet::load(const std::string& wallet_file) {
        std::ifstream wal_file(wallet_file, std::ios::binary | std::ios::in);
        if(wal_file.good()) {
            auto deser = istream_serializer(wal_file);
            {
                std::unique_lock<std::shared_mutex> lk(m_keys_mut);

                m_keys.clear();
                m_pubkeys.clear();
                m_witness_programs.clear();

                deser >> m_keys;
                for(const auto& k : m_keys) {
                    m_pubkeys.push_back(k.first);
                    m_witness_programs.insert(
                        {transaction::validation::get_p2pk_witness_commitment(
                             k.first),
                         k.first});
                }
            }

            {
                std::unique_lock<std::shared_mutex> lu(m_utxos_mut);

                m_utxos_set.clear();
                m_spend_queue.clear();
                m_balance = 0;

                deser >> m_utxos_set;
                for(const auto& utxo : m_utxos_set) {
                    m_balance += utxo.m_prevout_data.m_value;
                    m_spend_queue.push_back(utxo);
                }
            }
        }
    }

    auto transaction::wallet::send_to(size_t input_count,
                                      size_t output_count,
                                      const pubkey_t& payee,
                                      bool sign_tx)
        -> std::optional<transaction::full_tx> {
        assert(input_count > 0);
        assert(output_count > 0);

        // TODO: handle overflow with large output values.
        uint64_t total_amount = 0;
        transaction::full_tx ret;
        uint64_t output_val{};

        {
            std::unique_lock<std::shared_mutex> ul(m_utxos_mut);
            if((m_utxos_set.size() + m_seed_to - m_seed_from) < input_count) {
                return std::nullopt;
            }

            ret.m_inputs.reserve(input_count);

            size_t seeded_inputs = 0;
            while(m_seed_from != m_seed_to
                  && ret.m_inputs.size() < input_count) {
                auto seed_utxo = create_seeded_input(m_seed_from);
                if(!seed_utxo) {
                    break;
                }
                ret.m_inputs.push_back(seed_utxo.value());
                ret.m_witness.emplace_back(sig_len, std::byte(0));
                total_amount += m_seed_value;
                m_seed_from++;
                seeded_inputs++;
            }

            for(auto utxo = m_spend_queue.begin();
                (utxo != m_spend_queue.end())
                && (ret.m_inputs.size() < input_count);
                utxo++) {
                ret.m_inputs.push_back(*utxo);
                total_amount += utxo->m_prevout_data.m_value;
            }

            output_val = total_amount / output_count;
            if(output_val == 0 && output_count > 1) {
                // Caller asked for more outputs than we can make with the
                // amount of coins we have.
                m_seed_from -= seeded_inputs;
                return std::nullopt;
            }

            for(size_t i = seeded_inputs; i < ret.m_inputs.size(); i++) {
                auto& inp = ret.m_inputs[i];
                m_balance -= inp.m_prevout_data.m_value;
                m_utxos_set.erase(inp);
                m_spend_queue.pop_front();
            }
        }

        auto wit_comm
            = transaction::validation::get_p2pk_witness_commitment(payee);
        ret.m_outputs.reserve(output_count);
        for(size_t i{0}; i < output_count; i++) {
            transaction::output send_out;
            if(i == output_count - 1) {
                send_out.m_value = total_amount;
            } else {
                send_out.m_value = output_val;
            }
            total_amount -= send_out.m_value;
            send_out.m_witness_program_commitment = wit_comm;
            ret.m_outputs.push_back(send_out);
        }

        assert(total_amount == 0);

        if(sign_tx) {
            sign(ret);
        }

        return ret;
    }

    void transaction::wallet::confirm_inputs(
        const std::vector<transaction::input>& credits) {
        update_balance(credits, {});
    }

    auto transaction::wallet::fan(size_t output_count,
                                  uint32_t value,
                                  const pubkey_t& payee,
                                  bool sign_tx)
        -> std::optional<transaction::full_tx> {
        const uint64_t amount = output_count * value;
        auto maybe_tx = accumulate_inputs(amount);
        if(!maybe_tx.has_value()) {
            return std::nullopt;
        }

        auto& ret = maybe_tx.value().first;
        auto total_amount = maybe_tx.value().second;

        if(total_amount > amount) {
            // Add the change output if we need to
            transaction::output change_out;
            change_out.m_value = total_amount - amount;
            const auto pubkey = generate_key();
            change_out.m_witness_program_commitment
                = transaction::validation::get_p2pk_witness_commitment(pubkey);
            ret.m_outputs.push_back(change_out);
        }

        transaction::output destination_out;
        destination_out.m_value = value;

        destination_out.m_witness_program_commitment
            = transaction::validation::get_p2pk_witness_commitment(payee);
        for(size_t i{0}; i < output_count; i++) {
            ret.m_outputs.push_back(destination_out);
        }

        if(sign_tx) {
            sign(ret);
        }

        return ret;
    }

    auto transaction::wallet::accumulate_inputs(uint64_t amount)
        -> std::optional<std::pair<full_tx, uint64_t>> {
        uint64_t total_amount = 0;
        auto ret = full_tx();
        {
            std::unique_lock<std::shared_mutex> ul(m_utxos_mut);
            size_t seeded_inputs = 0;
            while(m_seed_from != m_seed_to && total_amount < amount) {
                auto seed_utxo = create_seeded_input(m_seed_from);
                if(!seed_utxo) {
                    break;
                }
                ret.m_inputs.push_back(seed_utxo.value());
                ret.m_witness.emplace_back(sig_len, std::byte(0));
                total_amount += m_seed_value;
                m_seed_from++;
                seeded_inputs++;
            }

            auto utxo = m_spend_queue.begin();
            while((total_amount < amount) && (utxo != m_spend_queue.end())) {
                ret.m_inputs.push_back(*utxo);
                ret.m_witness.emplace_back(sig_len, std::byte(0));
                total_amount += utxo->m_prevout_data.m_value;
                std::advance(utxo, 1);
            }

            if(total_amount < amount) {
                m_seed_from -= seeded_inputs;
                return std::nullopt;
            }

            for(size_t i = seeded_inputs; i < ret.m_inputs.size(); i++) {
                const auto del_utxo = m_spend_queue.begin();
                m_balance -= del_utxo->m_prevout_data.m_value;
                m_utxos_set.erase(*del_utxo);
                m_spend_queue.pop_front();
            }
        }
        return {{ret, total_amount}};
    }
}
