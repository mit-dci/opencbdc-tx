// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evm_bench.hpp"

#include "contracts.hpp"
#include "parsec/agent/runners/evm/address.hpp"
#include "parsec/agent/runners/evm/format.hpp"
#include "parsec/agent/runners/evm/math.hpp"
#include "parsec/agent/runners/evm/serialization.hpp"
#include "parsec/agent/runners/evm/signature.hpp"
#include "util/serialization/util.hpp"

using namespace cbdc::parsec::agent::runner;

evm_bench::evm_bench(size_t loadgen_id,
                     size_t mint_tree_depth,
                     cbdc::parsec::config cfg,
                     std::shared_ptr<cbdc::logging::log> log,
                     std::shared_ptr<geth_client> client)
    : m_loadgen_id(loadgen_id),
      m_cfg(std::move(cfg)),
      m_log(std::move(log)),
      m_client(std::move(client)) {
    auto skey_buf = cbdc::buffer::from_hex("32a49a8408806e7a2862bca482c7aabd27"
                                           "e846f673edc8fb14501cab0d1d8ebe")
                        .value();
    std::memcpy(skey_buf.data_at(skey_buf.size() - sizeof(m_loadgen_id)),
                &m_loadgen_id,
                sizeof(m_loadgen_id));
    std::memcpy(m_skey.data(), skey_buf.data(), sizeof(m_skey));

    m_total_accounts = (1 << mint_tree_depth);

    m_log->debug("Total Accounts will be ",m_total_accounts);

    // There are 2^mint_tree_depth-1 accounts in the mint tree,
    // Without the -1 below, the root of the mint tree would have
    // 2*val_per_acc in its account.
    m_total_mint = evmc::uint256be(m_total_accounts - 1) * m_val_per_acc;

    m_log->debug("Total Mint will be ", cbdc::parsec::agent::runner::to_hex(m_total_mint));

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
    auto tx = cbdc::parsec::agent::runner::evm_tx();
    tx.m_to = to_addr;
    tx.m_value = value;
    tx.m_gas_limit = evmc::uint256be(0xffffffff);
    tx.m_gas_price = evmc::uint256be(0);
    tx.m_nonce = nonce;
    tx.m_type = cbdc::parsec::agent::runner::evm_tx_type::legacy;

    auto sighash = cbdc::parsec::agent::runner::sig_hash(tx);
    auto sig = cbdc::parsec::agent::runner::eth_sign(skey,
                                                     sighash,
                                                     tx.m_type,
                                                     m_secp_context);

    tx.m_sig = sig;

    auto tx_buf = cbdc::parsec::agent::runner::tx_encode(tx);
    auto tx_hex = "0x" + tx_buf.to_hex();
    return tx_hex;
}

auto evm_bench::release_escrow( evmc::address contract_address,
                        evmc::uint256be deal_id,
                        evmc::uint256be nonce,
                       cbdc::privkey_t skey) -> std::string {
    auto tx = cbdc::parsec::agent::runner::evm_tx();
    tx.m_to = contract_address;
    //tx.m_value = value;
    tx.m_gas_limit = evmc::uint256be(0xffffffff);
    tx.m_gas_price = evmc::uint256be(0);
    tx.m_nonce = nonce;
    tx.m_type = cbdc::parsec::agent::runner::evm_tx_type::legacy;
     auto data
        = cbdc::parsec::evm_contracts::data_myescrow_release(deal_id);
    tx.m_input.resize(data.size());
    std::memcpy(tx.m_input.data(), data.data(), data.size());
    auto sighash = cbdc::parsec::agent::runner::sig_hash(tx);
    auto sig = cbdc::parsec::agent::runner::eth_sign(skey,
                                                     sighash,
                                                     tx.m_type,
                                                     m_secp_context);

    tx.m_sig = sig;

    auto tx_buf = cbdc::parsec::agent::runner::tx_encode(tx);
    auto tx_hex = "0x" + tx_buf.to_hex();
    return tx_hex;
}



auto evm_bench::deploy_erc20(evmc::uint256be nonce, cbdc::privkey_t skey)
    -> std::string {
    auto tx = cbdc::parsec::agent::runner::evm_tx();
    tx.m_gas_limit = evmc::uint256be(0xffffffff);
    tx.m_gas_price = evmc::uint256be(0);
    tx.m_nonce = nonce;
    tx.m_type = cbdc::parsec::agent::runner::evm_tx_type::legacy;
    auto data = cbdc::parsec::evm_contracts::data_erc20_deploy();
    tx.m_input.resize(data.size());
    std::memcpy(tx.m_input.data(), data.data(), data.size());
    auto sighash = cbdc::parsec::agent::runner::sig_hash(tx);
    auto sig = cbdc::parsec::agent::runner::eth_sign(skey,
                                                     sighash,
                                                     tx.m_type,
                                                     m_secp_context);

    tx.m_sig = sig;

    auto tx_buf = cbdc::parsec::agent::runner::tx_encode(tx);
    auto tx_hex = "0x" + tx_buf.to_hex();
    return tx_hex;
}

auto evm_bench::deploy_escrow(evmc::uint256be nonce, cbdc::privkey_t skey)
    -> std::string {
    auto tx = cbdc::parsec::agent::runner::evm_tx();
    tx.m_gas_limit = evmc::uint256be(0xffffffff);
    tx.m_gas_price = evmc::uint256be(0);
    tx.m_nonce = nonce;
    tx.m_type = cbdc::parsec::agent::runner::evm_tx_type::legacy;
    auto data = cbdc::parsec::evm_contracts::data_myescrow_deploy();
    tx.m_input.resize(data.size());
    std::memcpy(tx.m_input.data(), data.data(), data.size());
    auto sighash = cbdc::parsec::agent::runner::sig_hash(tx);
    auto sig = cbdc::parsec::agent::runner::eth_sign(skey,
                                                     sighash,
                                                     tx.m_type,
                                                     m_secp_context);

    tx.m_sig = sig;

    auto tx_buf = cbdc::parsec::agent::runner::tx_encode(tx);
    auto tx_hex = "0x" + tx_buf.to_hex();
    return tx_hex;
}

auto evm_bench::send_erc20(evmc::address erc20_addr,
                           evmc::uint256be nonce,
                           evmc::address to_addr,
                           cbdc::privkey_t skey,
                           evmc::uint256be value) -> std::string {
    auto tx = cbdc::parsec::agent::runner::evm_tx();
    tx.m_to = erc20_addr;
    tx.m_gas_limit = evmc::uint256be(0xffffffff);
    tx.m_gas_price = evmc::uint256be(0);
    tx.m_nonce = nonce;
    tx.m_type = cbdc::parsec::agent::runner::evm_tx_type::legacy;
    auto data
        = cbdc::parsec::evm_contracts::data_erc20_transfer(to_addr, value);
    tx.m_input.resize(data.size());
    std::memcpy(tx.m_input.data(), data.data(), data.size());
    auto sighash = cbdc::parsec::agent::runner::sig_hash(tx);
    auto sig = cbdc::parsec::agent::runner::eth_sign(skey,
                                                     sighash,
                                                     tx.m_type,
                                                     m_secp_context);

    tx.m_sig = sig;

    auto tx_buf = cbdc::parsec::agent::runner::tx_encode(tx);
    auto tx_hex = "0x" + tx_buf.to_hex();
    return tx_hex;
}

auto evm_bench::deposit_escrow(evmc::address contract_address,
                           evmc::uint256be nonce,
                           evmc::address arbiter_addr,
                            evmc::address seller_addr,
                           cbdc::privkey_t skey,
                           evmc::uint256be value) -> std::string {
    auto tx = cbdc::parsec::agent::runner::evm_tx();
    tx.m_to = contract_address;
    tx.m_gas_limit = evmc::uint256be(0xffffffff);
    tx.m_gas_price = evmc::uint256be(0);
    tx.m_nonce = nonce;
    tx.m_value = value;
    tx.m_type = cbdc::parsec::agent::runner::evm_tx_type::legacy;
    auto data
        = cbdc::parsec::evm_contracts::data_myescrow_deposit(seller_addr, arbiter_addr);
    tx.m_input.resize(data.size());
    std::memcpy(tx.m_input.data(), data.data(), data.size());
    auto sighash = cbdc::parsec::agent::runner::sig_hash(tx);
    auto sig = cbdc::parsec::agent::runner::eth_sign(skey,
                                                     sighash,
                                                     tx.m_type,
                                                     m_secp_context);

    tx.m_sig = sig;

    auto tx_buf = cbdc::parsec::agent::runner::tx_encode(tx);
    auto tx_hex = "0x" + tx_buf.to_hex();
    return tx_hex;
}

auto evm_bench::new_account() -> std::pair<cbdc::privkey_t, evmc::address> {
    auto new_skey = m_rnd.random_hash();
    auto addr
        = cbdc::parsec::agent::runner::eth_addr(new_skey, m_secp_context);
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
        = cbdc::parsec::agent::runner::eth_addr(acc_skey, m_secp_context);
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
    if(m_cfg.m_load_type == cbdc::parsec::load_type::transfer || m_cfg.m_load_type == cbdc::parsec::load_type::escrow) {
        mint_tx_hex
            = gen_tx(evmc::uint256be(count), new_addr, acc_skey, mint_amt);
    } else if(m_cfg.m_load_type == cbdc::parsec::load_type::erc20) {
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
        = cbdc::parsec::agent::runner::eth_addr(m_skey, m_secp_context);

    m_log->info("Using privkey [",
                cbdc::to_string(m_skey),
                "] (address [",
                cbdc::parsec::agent::runner::to_hex(from_addr),
                "])");

    m_in_flight++;
    m_client->get_transaction_count(
        cbdc::parsec::agent::runner::to_hex(from_addr),
        [this, from_addr](std::optional<evmc::uint256be> maybe_nonce) {
            m_in_flight--;
   
            if(!maybe_nonce.has_value()) {
                m_log->error("Error retrieving transaction count");
                m_done = true;
                return;
            }

            m_log->info("Received Response & nonce value is  ");

            m_current_nonce = maybe_nonce.value();
            auto tx_hex = std::string();
            if(m_cfg.m_load_type == cbdc::parsec::load_type::transfer) {
                tx_hex = gen_tx(m_current_nonce,
                                m_init_addr,
                                m_skey,
                                m_total_mint);
            } else if(m_cfg.m_load_type == cbdc::parsec::load_type::erc20) {
                tx_hex = deploy_erc20(m_current_nonce, m_skey);
            } else if(m_cfg.m_load_type == cbdc::parsec::load_type::escrow) {
                tx_hex = deploy_escrow(m_current_nonce, m_skey);
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

                    if(m_cfg.m_load_type == cbdc::parsec::load_type::transfer) {
                        m_done = true;
                        m_success = true;
                        return;
                    }
                    if(m_cfg.m_load_type
                       == cbdc::parsec::load_type::erc20) {
                        m_erc20_addr
                            = cbdc::parsec::agent::runner::contract_address(
                                from_addr,
                                m_current_nonce);
                        m_log->info(
                            "Deployed ERC20 to",
                            cbdc::parsec::agent::runner::to_hex(m_erc20_addr));

                        auto mint_tx_hex
                            = send_erc20(m_erc20_addr,
                                         m_current_nonce + evmc::uint256be(1),
                                         m_init_addr,
                                         m_skey,
                                         m_total_mint);
                        m_in_flight++;
                        m_client->send_transaction(
                            mint_tx_hex,
                            [this](
                                const std::optional<std::string>& send_res) {
                                m_in_flight--;
                                if(!send_res.has_value()) {
                                    m_log->error("Error sending transaction");
                                } else {
                                    m_success = true;
                                    m_balances[m_init_addr] = m_total_mint;
                                }
                                m_done = true;
                            });
                    }

                    if(m_cfg.m_load_type
                       == cbdc::parsec::load_type::escrow) {
                        m_escrow_addr
                            = cbdc::parsec::agent::runner::contract_address(
                                from_addr,
                                m_current_nonce);
                        m_log->info(
                            "Deployed Escrow to",
                            cbdc::parsec::agent::runner::to_hex(m_escrow_addr));

                        auto mint_tx_hex
                            = gen_tx(m_current_nonce + evmc::uint256be(1),
                                m_init_addr,
                                m_skey,
                                m_total_mint);
                            // send_erc20(m_escrow_addr,
                            //              m_current_nonce + evmc::uint256be(1),
                            //              m_init_addr,
                            //              m_skey,
                            //              m_total_mint);
                        m_in_flight++;
                        m_client->send_transaction(
                            mint_tx_hex,
                            [this](
                                const std::optional<std::string>& send_res) {
                                m_in_flight--;
                                if(!send_res.has_value()) {
                                    m_log->error("Error sending transaction");
                                } else {
                                    m_success = true;
                                    m_balances[m_init_addr] = m_total_mint;
                                }
                                m_done = true;
                            });
                     }
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
        m_log->trace(cbdc::parsec::agent::runner::to_hex(tx_from_addr),
                     "has insufficient balance, reclaiming",
                     cbdc::parsec::agent::runner::to_hex(send_amt),
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
    if(m_cfg.m_load_type == cbdc::parsec::load_type::transfer) {
        send_tx_hex = gen_tx(nonce, to_addr, acc_skey, send_amt);
    } else if(m_cfg.m_load_type == cbdc::parsec::load_type::erc20) {
        send_tx_hex
            = send_erc20(m_erc20_addr, nonce, to_addr, acc_skey, send_amt);
    }
    nonce = nonce + evmc::uint256be(1);
    m_log->trace("TX Schedule: ", cbdc::parsec::agent::runner::to_hex(tx_from_addr),
                     "sends ",
                     cbdc::parsec::agent::runner::to_hex(send_amt),
                     " to ", cbdc::parsec::agent::runner::to_hex(to_addr));
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
            m_in_flight--;
            m_log->trace("TX Schedule Returned Successfully ");
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


void evm_bench::schedule_escrow(size_t from, size_t seller, size_t to) {
    m_log->info("Scheduling escrow contracts with buyer ",from, ", arbiter ", to);
    auto send_amt = evmc::uint256be(1);
    auto& tx_from_addr = m_accounts[from].second;
    auto original_to = to;
    auto original_from = from;
    if(m_balances[tx_from_addr] < send_amt) {
        // When balance is too low, claim back what this account sent
        // to account 0 for m_contention_rate
        send_amt = m_sent_to_zero[tx_from_addr];
        m_log->trace(cbdc::parsec::agent::runner::to_hex(tx_from_addr),
                     "has insufficient balance, reclaiming",
                     cbdc::parsec::agent::runner::to_hex(send_amt),
                     "from account zero");
        return;
    }

    // m_log->trace(cbdc::parsec::agent::runner::to_hex(tx_from_addr),
    //                  " depositing ",
    //                  cbdc::parsec::agent::runner::to_hex(send_amt), 
    //                  " Balance was ", cbdc::parsec::agent::runner::to_hex(m_balances[tx_from_addr]));

    auto& acc_skey = m_accounts[from].first;
    auto& to_addr = m_accounts[to].second;
    auto& seller_addr = m_accounts[seller].second;
    auto& nonce = m_nonces[from];
    std::string send_tx_hex{};

    //m_log->trace("sending Escrow's deposit request ");
        send_tx_hex
            = deposit_escrow(m_escrow_addr, nonce, to_addr, seller_addr, acc_skey, send_amt);

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
         seller,
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

            // Do it here
            m_log->trace("Escrow's deposit exe success with buyer:", from, ", arbiter:", to); 
            //m_log->trace(" TX executed successfully with new balance ", cbdc::parsec::agent::runner::to_hex(from_bal));
            m_in_flight++;
             m_client->get_transaction_receipt(
                maybe_txid.value(),
                [this, to, seller, send_amt](std::optional<std::string> value) {
                     m_txs++;
                    if(!value.has_value()) {
                        m_log->error("Error sending TX");
                        m_error = true;
                        return;
                    }
                    //m_log->trace("\t deal-id:", value.value());
                    auto deal_id = cbdc::parsec::agent::runner::uint256be_from_hex(value.value());
                    schedule_escrow_release(to, 
                        deal_id.value(), 
                        seller, send_amt);

                    m_in_flight--;    
                 });
            // 

            auto end_time = std::chrono::high_resolution_clock::now();
            auto latency = (end_time - start_time).count();
            m_in_flight--;
            m_samples_file << end_time.time_since_epoch().count() << " "
                           << latency << "\n";

            if(m_running) {
                if(to != original_to) {
                    schedule_tx(original_from, original_to);
                    return;
                }
                //schedule_tx(to, from);
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


void evm_bench::schedule_escrow_release(size_t from /*arbiter*/, evmc::uint256be deal_id, size_t seller, evmc::uint256be amount) {
    //auto& tx_from_addr = m_accounts[from].second;
    auto& seller_addr = m_accounts[seller].second;
    auto& acc_skey = m_accounts[from].first;
    auto& nonce = m_nonces[from];
    
    std::string send_tx_hex{};
    send_tx_hex
            = release_escrow(m_escrow_addr, deal_id, nonce, acc_skey);
    nonce = nonce + evmc::uint256be(1);
    // m_log->trace("TX Escrow Release,TXN: ", send_tx_hex);
    // m_log->trace("Seller Address Balance was: ", cbdc::parsec::agent::runner::to_hex(m_balances[seller_addr]));
    // m_log->trace("Escrow Arbiter Private Key: ", cbdc::to_string(acc_skey));
    // m_log->trace("Escrow Arbiter Pub Key: ", cbdc::parsec::agent::runner::to_hex(tx_from_addr));
    
    auto start_time = std::chrono::high_resolution_clock::now();
    m_in_flight++;
    m_client->send_transaction(
        send_tx_hex,
        [this,from, seller,
        amount,
         seller_addr,
         start_time](std::optional<std::string> maybe_txid) {
            m_txs++;
            if(!maybe_txid.has_value()) {
                m_log->error("Error sending TX");
                m_error = true;
                return;
            }
            auto& seller_bal = m_balances[seller_addr];
            seller_bal = seller_bal + evmc::uint256be(amount);
            m_log->trace("Escrow's release exe success with arbiter: ", from, ", seller: ", seller );
            //m_log->trace("TX Release Returned Successfully with new seller balance ", cbdc::parsec::agent::runner::to_hex(seller_bal));

            auto end_time = std::chrono::high_resolution_clock::now();
            auto latency = (end_time - start_time).count();
            m_in_flight--;
            m_samples_file << end_time.time_since_epoch().count() << " "
                           << latency << "\n";

            // m_in_flight++;
            // m_client->get_balance(
            //     cbdc::parsec::agent::runner::to_hex(seller_addr),
            //     [this, seller_addr](std::optional<evmc::uint256be> maybe_balance) {
            //         m_in_flight--;
        
            //         if(!maybe_balance.has_value()) {
            //             m_log->error("Error retrieving transaction count");
            //             m_done = true;
            //             return;
            //         }

            //         m_log->info("Server [",cbdc::parsec::agent::runner::to_hex(seller_addr),"] balance value is  ", cbdc::parsec::agent::runner::to_hex(maybe_balance.value()));
            //     });

        });
}

