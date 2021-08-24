// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evm_bench.hpp"

#include "3pc/agent/runners/evm/address.hpp"
#include "3pc/agent/runners/evm/format.hpp"
#include "3pc/agent/runners/evm/math.hpp"
#include "3pc/agent/runners/evm/serialization.hpp"
#include "3pc/agent/runners/evm/signature.hpp"
#include "contracts.hpp"
#include "util/serialization/util.hpp"

using namespace cbdc::threepc::agent::runner;

evm_bench::evm_bench(size_t loadgen_id,
                     size_t mint_tree_depth,
                     cbdc::threepc::config cfg,
                     std::shared_ptr<cbdc::logging::log> log,
                     std::shared_ptr<geth_client> client,
                     std::shared_ptr<cbdc::telemetry> tel)
    : m_loadgen_id(loadgen_id),
      m_cfg(std::move(cfg)),
      m_log(std::move(log)),
      m_client(std::move(client)),
      m_tel(std::move(tel)) {
    auto skey_buf = cbdc::buffer::from_hex("32a49a8408806e7a2862bca482c7aabd27"
                                           "e846f673edc8fb14501cab0d1d8ebe")
                        .value();
    std::memcpy(skey_buf.data_at(skey_buf.size() - sizeof(m_loadgen_id)),
                &m_loadgen_id,
                sizeof(m_loadgen_id));
    std::memcpy(m_skey.data(), skey_buf.data(), sizeof(m_skey));

    m_total_accounts = (1 << mint_tree_depth);

    // There are 2^mint_tree_depth-1 accounts in the mint tree,
    // Without the -1 below, the root of the mint tree would have
    // 2*val_per_acc in its account.
    m_total_mint = evmc::uint256be(m_total_accounts - 1) * m_val_per_acc;

    auto acc = new_account();
    m_init_addr = acc.second;
    m_init_skey = acc.first;

    m_contention_dist = decltype(m_contention_dist)(cfg.m_contention_rate);

    m_samples_file
        = std::ofstream("tx_samples_" + std::to_string(m_loadgen_id) + ".txt",
                        std::ios::trunc);
}

auto evm_bench::gen_tx(evmc::uint256be nonce,
                       evmc::address to_addr,
                       cbdc::privkey_t skey,
                       evmc::uint256be value) -> std::string {
    auto tx = cbdc::threepc::agent::runner::evm_tx();
    tx.m_to = to_addr;
    tx.m_value = value;
    tx.m_gas_limit = evmc::uint256be(0xffffffff);
    tx.m_gas_price = evmc::uint256be(0);
    tx.m_nonce = nonce;
    tx.m_type = cbdc::threepc::agent::runner::evm_tx_type::legacy;

    auto sighash = cbdc::threepc::agent::runner::sig_hash(tx);
    auto sig = cbdc::threepc::agent::runner::eth_sign(skey,
                                                      sighash,
                                                      tx.m_type,
                                                      m_secp_context);

    tx.m_sig = sig;

    auto tx_buf = cbdc::threepc::agent::runner::tx_encode(tx);
    auto tx_hex = "0x" + tx_buf.to_hex();
    return tx_hex;
}

auto evm_bench::deploy_erc20(evmc::uint256be nonce, cbdc::privkey_t skey)
    -> std::string {
    auto tx = cbdc::threepc::agent::runner::evm_tx();
    tx.m_gas_limit = evmc::uint256be(0xffffffff);
    tx.m_gas_price = evmc::uint256be(0);
    tx.m_nonce = nonce;
    tx.m_type = cbdc::threepc::agent::runner::evm_tx_type::legacy;
    auto data = cbdc::threepc::evm_contracts::data_erc20_deploy();
    tx.m_input.resize(data.size());
    std::memcpy(tx.m_input.data(), data.data(), data.size());
    auto sighash = cbdc::threepc::agent::runner::sig_hash(tx);
    auto sig = cbdc::threepc::agent::runner::eth_sign(skey,
                                                      sighash,
                                                      tx.m_type,
                                                      m_secp_context);

    tx.m_sig = sig;

    auto tx_buf = cbdc::threepc::agent::runner::tx_encode(tx);
    auto tx_hex = "0x" + tx_buf.to_hex();
    return tx_hex;
}

auto evm_bench::send_erc20(evmc::address erc20_addr,
                           evmc::uint256be nonce,
                           evmc::address to_addr,
                           cbdc::privkey_t skey,
                           evmc::uint256be value) -> std::string {
    auto tx = cbdc::threepc::agent::runner::evm_tx();
    tx.m_to = erc20_addr;
    tx.m_gas_limit = evmc::uint256be(0xffffffff);
    tx.m_gas_price = evmc::uint256be(0);
    tx.m_nonce = nonce;
    tx.m_type = cbdc::threepc::agent::runner::evm_tx_type::legacy;
    auto data
        = cbdc::threepc::evm_contracts::data_erc20_transfer(to_addr, value);
    tx.m_input.resize(data.size());
    std::memcpy(tx.m_input.data(), data.data(), data.size());
    auto sighash = cbdc::threepc::agent::runner::sig_hash(tx);
    auto sig = cbdc::threepc::agent::runner::eth_sign(skey,
                                                      sighash,
                                                      tx.m_type,
                                                      m_secp_context);

    tx.m_sig = sig;

    auto tx_buf = cbdc::threepc::agent::runner::tx_encode(tx);
    auto tx_hex = "0x" + tx_buf.to_hex();
    return tx_hex;
}

auto evm_bench::new_account() -> std::pair<cbdc::privkey_t, evmc::address> {
    auto new_skey = m_rnd.random_hash();
    auto addr
        = cbdc::threepc::agent::runner::eth_addr(new_skey, m_secp_context);
    return {new_skey, addr};
}

void evm_bench::mint_tree(size_t depth, cbdc::privkey_t acc_skey) {
    auto mint_amt = evmc::uint256be((1 << depth) - 1) * m_val_per_acc;
    mint_one(1, mint_amt, acc_skey, depth);
}

void evm_bench::mint_tree(size_t depth) {
    m_log->info("Minting", m_total_accounts, "accounts");
    mint_tree(depth, m_init_skey);
}

void evm_bench::mint_one(size_t count,
                         evmc::uint256be mint_amt,
                         cbdc::privkey_t acc_skey,
                         size_t depth) {
    auto tx_from_addr
        = cbdc::threepc::agent::runner::eth_addr(acc_skey, m_secp_context);
    auto new_acc = new_account();
    auto& new_skey = new_acc.first;
    auto& new_addr = new_acc.second;
    auto& from_bal = m_balances[tx_from_addr];
    from_bal = from_bal - mint_amt;
    m_balances[new_addr] = mint_amt;

    constexpr auto fan_out = 2;

    m_accounts.emplace_back(std::make_pair(new_skey, new_addr));
    if(depth > 1) {
        m_nonces.emplace_back(evmc::uint256be(fan_out + 1));
    } else {
        m_nonces.emplace_back(evmc::uint256be(1));
    }

    std::string mint_tx_hex{};
    if(m_cfg.m_load_type == cbdc::threepc::load_type::transfer) {
        mint_tx_hex
            = gen_tx(evmc::uint256be(count), new_addr, acc_skey, mint_amt);
    } else if(m_cfg.m_load_type == cbdc::threepc::load_type::erc20) {
        mint_tx_hex = send_erc20(m_erc20_addr,
                                 evmc::uint256be(count),
                                 new_addr,
                                 acc_skey,
                                 mint_amt);
    }

    m_in_flight++;
    m_client->send_transaction(
        mint_tx_hex,
        [this, depth, new_skey, count, acc_skey, mint_amt, new_addr](
            const std::optional<std::string>& maybe_txid) {
            if(!maybe_txid.has_value()) {
                m_log->error("Mint TX had error");
                m_success = false;
                m_in_flight = 0;
                return;
            }

            m_in_flight--;
            m_balances[new_addr] = mint_amt;

            if(count < fan_out) {
                mint_one(count + 1, mint_amt, acc_skey, depth);
            }

            if(depth > 1) {
                mint_tree(depth - 1, new_skey);
            }
        });
}

void evm_bench::deploy() {
    auto from_addr
        = cbdc::threepc::agent::runner::eth_addr(m_skey, m_secp_context);

    m_log->info("Using privkey [",
                cbdc::to_string(m_skey),
                "] (address [",
                cbdc::threepc::agent::runner::to_hex(from_addr),
                "])");

    m_in_flight++;
    m_client->get_transaction_count(
        cbdc::threepc::agent::runner::to_hex(from_addr),
        [this, from_addr](std::optional<evmc::uint256be> maybe_nonce) {
            m_in_flight--;
            if(!maybe_nonce.has_value()) {
                m_log->error("Error retrieving transaction count");
                m_done = true;
                return;
            }

            m_current_nonce = maybe_nonce.value();
            auto tx_hex = std::string();
            if(m_cfg.m_load_type == cbdc::threepc::load_type::transfer) {
                tx_hex = gen_tx(m_current_nonce,
                                m_init_addr,
                                m_skey,
                                m_total_mint);
            } else if(m_cfg.m_load_type == cbdc::threepc::load_type::erc20) {
                tx_hex = deploy_erc20(m_current_nonce, m_skey);
            }

            m_in_flight++;
            m_client->send_transaction(
                tx_hex,
                [this,
                 from_addr](const std::optional<std::string>& maybe_txid) {
                    m_in_flight--;
                    if(!maybe_txid.has_value()) {
                        m_log->error("Error sending transaction");
                        m_done = true;
                        return;
                    }

                    if(m_cfg.m_load_type != cbdc::threepc::load_type::erc20) {
                        m_done = true;
                        m_success = true;
                        return;
                    }

                    m_erc20_addr
                        = cbdc::threepc::agent::runner::contract_address(
                            from_addr,
                            m_current_nonce);
                    m_log->info(
                        "Deployed ERC20 to",
                        cbdc::threepc::agent::runner::to_hex(m_erc20_addr));
                    auto mint_tx_hex
                        = send_erc20(m_erc20_addr,
                                     m_current_nonce + evmc::uint256be(1),
                                     m_init_addr,
                                     m_skey,
                                     m_total_mint);
                    m_in_flight++;
                    m_client->send_transaction(
                        mint_tx_hex,
                        [this](const std::optional<std::string>& send_res) {
                            m_in_flight--;
                            if(!send_res.has_value()) {
                                m_log->error("Error sending transaction");
                            } else {
                                m_success = true;
                                m_balances[m_init_addr] = m_total_mint;
                            }
                            m_done = true;
                        });
                });
        });
}

void evm_bench::schedule_tx(size_t from, size_t to) {
    auto send_amt = evmc::uint256be(1);
    auto& tx_from_addr = m_accounts[from].second;
    auto original_to = to;
    auto original_from = from;
    if(m_balances[tx_from_addr] < send_amt) {
        // When balance is too low, claim back what this account sent
        // to account 0 for m_contention_rate
        send_amt = m_sent_to_zero[tx_from_addr];
        m_log->trace(cbdc::threepc::agent::runner::to_hex(tx_from_addr),
                     "has insufficient balance, reclaiming",
                     cbdc::threepc::agent::runner::to_hex(send_amt),
                     "from account zero");
        to = from;
        from = 0;
        tx_from_addr = m_accounts[from].second;
    } else if(m_contention_dist(m_engine)) {
        // For m_contention_rate portion of transactions,
        // send to account 0
        to = 0;
    }
    auto& acc_skey = m_accounts[from].first;
    auto& to_addr = m_accounts[to].second;
    auto& nonce = m_nonces[from];
    std::string send_tx_hex{};
    if(m_cfg.m_load_type == cbdc::threepc::load_type::transfer) {
        send_tx_hex = gen_tx(nonce, to_addr, acc_skey, send_amt);
    } else if(m_cfg.m_load_type == cbdc::threepc::load_type::erc20) {
        send_tx_hex
            = send_erc20(m_erc20_addr, nonce, to_addr, acc_skey, send_amt);
    }
    nonce = nonce + evmc::uint256be(1);
    auto start_time = std::chrono::high_resolution_clock::now();
    m_in_flight++;
    m_client->send_transaction(
        send_tx_hex,
        [this,
         from,
         to,
         start_time,
         tx_from_addr,
         to_addr,
         send_amt,
         original_to,
         original_from](std::optional<std::string> maybe_txid) {
            m_txs++;
            if(!maybe_txid.has_value()) {
                m_log->error("Error sending TX");
                m_error = true;
                return;
            }
            auto& from_bal = m_balances[tx_from_addr];
            if(to == 0) {
                auto& from_sent_to_zero = m_sent_to_zero[tx_from_addr];
                from_sent_to_zero
                    = from_sent_to_zero + evmc::uint256be(send_amt);
            }
            if(from == 0) {
                auto& to_sent_to_zero = m_sent_to_zero[to_addr];
                to_sent_to_zero = to_sent_to_zero - evmc::uint256be(send_amt);
            }
            auto& to_bal = m_balances[to_addr];
            from_bal = from_bal - evmc::uint256be(send_amt);
            to_bal = to_bal + evmc::uint256be(send_amt);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto latency = (end_time - start_time).count();
            if(m_tel) {
                auto txid = cbdc::hash_from_hex(maybe_txid.value());
                m_tel->log(
                    "send_transaction",
                    cbdc::telemetry_details{
                        {cbdc::telemetry_keys::txid, txid},
                        {cbdc::telemetry_keys::address,
                         cbdc::make_buffer(tx_from_addr)},
                        {cbdc::telemetry_keys::address2,
                         cbdc::make_buffer(to_addr)},
                    },
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        start_time.time_since_epoch())
                        .count());
                m_tel->log(
                    "confirm_transaction",
                    cbdc::telemetry_details{
                        {cbdc::telemetry_keys::txid, txid},
                        {cbdc::telemetry_keys::latency, latency}},
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        end_time.time_since_epoch())
                        .count());
            }
            m_in_flight--;
            m_samples_file << end_time.time_since_epoch().count() << " "
                           << latency << "\n";

            if(m_running) {
                if(to != original_to) {
                    schedule_tx(original_from, original_to);
                    return;
                }
                schedule_tx(to, from);
            }
        });
}

void evm_bench::stop() {
    m_running = false;
}

auto evm_bench::pump() -> std::optional<bool> {
    if(m_error) {
        return false;
    }
    if(m_done) {
        m_done = false;
        return m_success;
    }
    if(m_in_flight == 0) {
        return !m_error;
    }
    if(!m_running) {
        return false;
    }

    auto success = m_client->pump();
    if(!success) {
        return false;
    }

    if(!m_start_time.has_value() && m_txs > 0) {
        m_start_time = std::chrono::high_resolution_clock::now();
    }
    if(m_txs > 1000) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = end_time - *m_start_time;
        auto txpns = m_txs / (duration.count() / 1000000000.0);
        m_log->info("TX/s:",
                    txpns,
                    "txs:",
                    m_txs,
                    "duration:",
                    duration.count());
        m_txs = 0;
        m_start_time = end_time;
    }

    return std::nullopt;
}

auto evm_bench::account_count() const -> size_t {
    return m_accounts.size();
}
