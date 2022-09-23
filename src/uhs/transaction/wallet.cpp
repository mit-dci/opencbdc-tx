// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.hpp"

#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/validation.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/istream_serializer.hpp"
#include "util/serialization/ostream_serializer.hpp"

#include <secp256k1_schnorrsig.h>

namespace cbdc {
    transaction::wallet::wallet() : m_log(nullptr) {
        init();
    }

    transaction::wallet::wallet(std::shared_ptr<logging::log> log)
        : m_log(std::move(log)) {
        init();
    }

    void transaction::wallet::init() {
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

            ret.m_outputs.push_back(out);
        }

        ret.m_out_spend_data
            = std::vector<spend_data>(n_outputs, {{}, output_val});

        [[maybe_unused]] auto res = transaction::add_proof(m_secp.get(),
                                                           m_generators.get(),
                                                           *m_random_source,
                                                           ret);

        assert(res);

        {
            auto id = transaction::tx_id(ret);
            std::unique_lock<std::shared_mutex> ul(m_utxos_mut);
            for(size_t i = 0; i < ret.m_outputs.size(); ++i) {
                transaction::output put = ret.m_outputs[i];
                put.m_range.reset();   // remove range proofs for inputs
                transaction::out_point point{id, i};
                transaction::input inp{point,
                                       put,
                                       ret.m_out_spend_data.value()[i]};
                const auto [_, inserted] = m_utxos_set.insert({point, inp});
                if(inserted) {
                    m_spend_queue.push_back(inp);
                }
            }
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

        std::vector<spend_data> out_spend_data{};

        transaction::output destination_out;
        out_spend_data.push_back(transaction::spend_data{{}, amount});

        destination_out.m_witness_program_commitment
            = transaction::validation::get_p2pk_witness_commitment(payee);
        ret.m_outputs.push_back(destination_out);

        if(total_amount > amount) {
            // Add the change output if we need to
            transaction::output change_out;
            const auto pubkey = generate_key();
            change_out.m_witness_program_commitment
                = transaction::validation::get_p2pk_witness_commitment(pubkey);
            ret.m_outputs.push_back(change_out);
            transaction::spend_data sp{{}, total_amount - amount};
            out_spend_data.push_back(sp);
        }

        ret.m_out_spend_data = out_spend_data;

        auto res = transaction::add_proof(m_secp.get(),
                                          m_generators.get(),
                                          *m_random_source,
                                          ret);

        if(!res) {
            return std::nullopt;
        }

        if(sign_tx) {
            sign(ret);
        }

        return ret;
    }

    auto transaction::wallet::create_seeded_transaction(size_t seed_idx,
        const commitment_t& comm,
        const rangeproof_t<>& range) -> std::optional<transaction::full_tx> {
        if(m_seed_from == m_seed_to) {
            return std::nullopt;
        }

        transaction::full_tx tx;
        tx.m_inputs.resize(1);
        tx.m_outputs.resize(1);

        transaction::input inp{};
        inp.m_prevout.m_tx_id = {0};
        inp.m_prevout.m_index = seed_idx;

        auto spend = transaction::spend_data{{}, m_seed_value};

        inp.m_prevout_data.m_witness_program_commitment = {0};
        inp.m_prevout_data.m_auxiliary = comm;
        inp.m_prevout_data.m_range.reset();
        inp.m_prevout_data.m_id = calculate_uhs_id(inp.m_prevout, inp.m_prevout_data, comm);
        inp.m_spend_data = {spend};

        tx.m_inputs[0] = inp;

        transaction::output outp{};
        outp.m_witness_program_commitment = m_seed_witness_commitment;
        outp.m_auxiliary = comm;
        outp.m_range = range;
        tx.m_outputs[0] = outp;

        auto outpoint = input_from_output(tx, 0).value().m_prevout;
        outp.m_id = calculate_uhs_id(outpoint, outp, comm);

        return tx;
    }

    auto transaction::wallet::create_seeded_transaction(size_t seed_idx)
        -> std::optional<transaction::full_tx> {
        if(m_seed_from == m_seed_to) {
            return std::nullopt;
        }

        transaction::full_tx tx;
        tx.m_inputs.resize(1);
        tx.m_outputs.resize(1);

        transaction::input inp{};
        inp.m_prevout.m_tx_id = {0};
        inp.m_prevout.m_index = seed_idx;

        inp.m_prevout_data.m_witness_program_commitment = {0};

        std::vector<transaction::spend_data> in_spend_data{};
        in_spend_data.push_back(transaction::spend_data{{}, m_seed_value});

        auto aux = transaction::roll_auxiliaries(m_secp.get(),
                                                 *m_random_source,
                                                 {},
                                                 in_spend_data);

        inp.m_prevout_data.m_auxiliary = serialize_commitment(m_secp.get(), aux.front());
        inp.m_prevout_data.m_id
            = transaction::calculate_uhs_id(inp.m_prevout,
                                            inp.m_prevout_data,
                                            inp.m_prevout_data.m_auxiliary);

        inp.m_spend_data = in_spend_data.front();
        tx.m_inputs[0] = inp;

        tx.m_outputs[0].m_witness_program_commitment
            = m_seed_witness_commitment;

        std::vector<transaction::spend_data> out_spend_data{};
        out_spend_data.push_back(transaction::spend_data{{}, m_seed_value});
        tx.m_out_spend_data = out_spend_data;
        auto res = transaction::add_proof(m_secp.get(),
                                          m_generators.get(),
                                          *m_random_source,
                                          tx);

        if(!res) {
            return std::nullopt;
        }

        return tx;
    }

    auto transaction::wallet::create_seeded_input(size_t seed_idx)
        -> std::optional<transaction::input> {
        if(m_seed_from == m_seed_to) {
            return std::nullopt;
        }

        auto maybe_tx = create_seeded_transaction(seed_idx);
        if(!maybe_tx.has_value()) {
            return std::nullopt;
        }
        auto tx = maybe_tx.value();

        if(!tx.m_out_spend_data.has_value()) {
            return std::nullopt;
        }

        auto maybe_inp = transaction::input_from_output(tx, 0);
        if(!maybe_inp.has_value()) {
            return std::nullopt;
        }
        auto inp = maybe_inp.value();

        inp.m_spend_data = tx.m_out_spend_data.value().front();

        return inp;
    }

    auto transaction::wallet::export_send_inputs(
        const transaction::full_tx& send_tx,
        const pubkey_t& payee) -> std::vector<transaction::input> {
        auto wit_comm
            = transaction::validation::get_p2pk_witness_commitment(payee);
        auto ret = std::vector<input>();
        for(uint32_t i = 0; i < send_tx.m_outputs.size(); i++) {
            if(send_tx.m_outputs[i].m_witness_program_commitment == wit_comm) {
                auto inp = transaction::input_from_output(send_tx, i).value();
                inp.m_spend_data = send_tx.m_out_spend_data.value()[i];
                ret.push_back(inp);
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

    auto
    transaction::wallet::spending_keys(const transaction::full_tx& tx) const
        -> std::optional<std::vector<std::pair<privkey_t, pubkey_t>>> {
        std::vector<std::pair<privkey_t, pubkey_t>> keys{};
        keys.reserve(tx.m_inputs.size());
        for(const auto& inp : tx.m_inputs) {
            const auto& wit_commit
                = inp.m_prevout_data.m_witness_program_commitment;

            {
                std::shared_lock<std::shared_mutex> sl(m_keys_mut);
                const auto wit_prog = m_witness_programs.find(wit_commit);
                if(wit_prog != m_witness_programs.end()) {
                    keys.emplace_back(m_keys.at(wit_prog->second),
                                      wit_prog->second);
                } else {
                    return std::nullopt;
                }
            }
        }
        return keys;
    }

    void transaction::wallet::sign(transaction::full_tx& tx) const {
        // TODO: other sighash types besides SIGHASH_ALL?
        const auto sighash = transaction::tx_id(tx);
        tx.m_witness.resize(tx.m_inputs.size());
        for(size_t i = 0; i < tx.m_inputs.size(); i++) {
            if(m_log) {
                m_log->info("Attempting to sign input", i);
            }
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
                if(m_log) {
                    m_log->info("Input", i, " is ours - signing");
                }

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
            } else {
                if(m_log) {
                    m_log->info("Input", i, " is not ours - not signing");
                }
            }
        }
    }

    void transaction::wallet::update_balance(
        const std::vector<transaction::input>& credits,
        const std::vector<transaction::input>& debits) {
        std::unique_lock<std::shared_mutex> lu(m_utxos_mut);
        for(const auto& inp : credits) {
            const auto [_, inserted]
                = m_utxos_set.insert({inp.m_prevout, inp});
            if(inserted) {
                m_spend_queue.push_back(inp);
            }
        }

        for(const auto& inp : debits) {
            m_utxos_set.erase(inp.m_prevout);
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
                    auto inp
                        = transaction::input_from_output(tx, i, tx_id).value();
                    inp.m_spend_data = tx.m_out_spend_data.value()[i];
                    new_utxos.push_back(inp);
                }
            }
        }
        update_balance(new_utxos, tx.m_inputs);
    }

    auto transaction::wallet::balance() const -> uint64_t {
        std::shared_lock<std::shared_mutex> lg(m_utxos_mut);
        // TODO: handle overflow
        uint64_t balance{0};
        for(const auto& [k, v] : m_utxos_set) {
            if(m_witness_programs.find(
                   v.m_prevout_data.m_witness_program_commitment)
               != m_witness_programs.end()) {
                balance += v.m_spend_data.value().m_value;
            }
        }
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

                deser >> m_utxos_set;
                for(const auto& [prevout, utxo] : m_utxos_set) {
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
                auto seed_inp = create_seeded_input(m_seed_from);
                if(!seed_inp) {
                    break;
                }
                ret.m_inputs.push_back(seed_inp.value());
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
                total_amount += utxo->m_spend_data.value().m_value;
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
                m_utxos_set.erase(inp.m_prevout);
                m_spend_queue.pop_front();
            }
        }

        std::vector<spend_data> out_spend_data{};

        auto wit_comm
            = transaction::validation::get_p2pk_witness_commitment(payee);
        ret.m_outputs.reserve(output_count);
        for(size_t i{0}; i < output_count; i++) {
            transaction::output send_out;
            uint64_t val{};
            if(i == output_count - 1) {
                val = total_amount;
            } else {
                val = output_val;
            }
            total_amount -= val;
            send_out.m_witness_program_commitment = wit_comm;
            ret.m_outputs.push_back(send_out);
            out_spend_data.push_back(transaction::spend_data{{}, val});
        }

        assert(total_amount == 0);

        ret.m_out_spend_data = out_spend_data;

        auto res = transaction::add_proof(m_secp.get(),
                                          m_generators.get(),
                                          *m_random_source,
                                          ret);

        if(!res) {
            return std::nullopt;
        }

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

        std::vector<spend_data> out_spend_data{};

        if(total_amount > amount) {
            // Add the change output if we need to
            transaction::output change_out;
            const auto pubkey = generate_key();
            change_out.m_witness_program_commitment
                = transaction::validation::get_p2pk_witness_commitment(pubkey);
            ret.m_outputs.push_back(change_out);
            transaction::spend_data sp{{}, total_amount - amount};
            out_spend_data.push_back(sp);
        }

        transaction::output destination_out;

        destination_out.m_witness_program_commitment
            = transaction::validation::get_p2pk_witness_commitment(payee);
        for(size_t i{0}; i < output_count; i++) {
            ret.m_outputs.push_back(destination_out);
            out_spend_data.push_back(transaction::spend_data{{}, value});
        }

        ret.m_out_spend_data = out_spend_data;

        auto res = transaction::add_proof(m_secp.get(),
                                          m_generators.get(),
                                          *m_random_source,
                                          ret);

        if(!res) {
            return std::nullopt;
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
                auto seed_inp = create_seeded_input(m_seed_from);
                if(!seed_inp) {
                    break;
                }
                ret.m_inputs.push_back(seed_inp.value());
                ret.m_witness.emplace_back(sig_len, std::byte(0));
                total_amount += m_seed_value;
                m_seed_from++;
                seeded_inputs++;
            }

            auto utxo = m_spend_queue.begin();
            while((total_amount < amount) && (utxo != m_spend_queue.end())) {
                ret.m_inputs.push_back(*utxo);
                ret.m_witness.emplace_back(sig_len, std::byte(0));
                total_amount += utxo->m_spend_data.value().m_value;
                std::advance(utxo, 1);
            }

            if(total_amount < amount) {
                m_seed_from -= seeded_inputs;
                return std::nullopt;
            }

            for(size_t i = seeded_inputs; i < ret.m_inputs.size(); i++) {
                const auto del_utxo = m_spend_queue.begin();
                m_utxos_set.erase(del_utxo->m_prevout);
                m_spend_queue.pop_front();
            }
        }

        return {{ret, total_amount}};
    }

    auto transaction::wallet::is_spendable(const transaction::input& in) const
        -> bool {
        const auto& in_key = in.m_prevout_data.m_witness_program_commitment;
        bool key_ours = false;
        {
            std::shared_lock<std::shared_mutex> sl(m_keys_mut);
            const auto wit_prog = m_witness_programs.find(in_key);
            key_ours = wit_prog != m_witness_programs.end();
        }
        return key_ours;
    }
}
