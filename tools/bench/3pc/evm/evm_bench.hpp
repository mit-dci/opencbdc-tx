// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_TOOLS_BENCH_3PC_EVM_EVM_BENCH_H_
#define OPENCBDC_TX_TOOLS_BENCH_3PC_EVM_EVM_BENCH_H_

#include "3pc/util.hpp"
#include "rpc_client.hpp"
#include "util/common/config.hpp"
#include "util/common/keys.hpp"
#include "util/common/random_source.hpp"

#include <atomic>
#include <evmc/evmc.hpp>
#include <memory>
#include <random>
#include <secp256k1.h>

class evm_bench {
  public:
    evm_bench(size_t loadgen_id,
              size_t mint_tree_depth,
              cbdc::threepc::config cfg,
              std::shared_ptr<cbdc::logging::log> log,
              std::shared_ptr<geth_client> client);

    void mint_tree(size_t depth);

    void deploy();

    void schedule_tx(size_t from, size_t to);

    void stop();

    auto pump() -> std::optional<bool>;

    auto account_count() const -> size_t;

  private:
    static constexpr size_t m_coins_per_account = 50;
    const evmc::uint256be m_val_per_acc{m_coins_per_account};

    std::shared_ptr<secp256k1_context> m_secp_context{
        secp256k1_context_create(SECP256K1_CONTEXT_SIGN
                                 | SECP256K1_CONTEXT_VERIFY),
        &secp256k1_context_destroy};
    cbdc::random_source m_rnd{cbdc::config::random_source};

    std::default_random_engine m_engine;
    std::bernoulli_distribution m_contention_dist;

    size_t m_loadgen_id;
    cbdc::threepc::config m_cfg;
    std::shared_ptr<cbdc::logging::log> m_log;
    std::shared_ptr<geth_client> m_client;
    std::unordered_map<evmc::address, evmc::uint256be> m_balances;
    std::vector<std::pair<cbdc::privkey_t, evmc::address>> m_accounts;
    std::vector<evmc::uint256be> m_nonces;
    std::unordered_map<evmc::address, evmc::uint256be> m_sent_to_zero;

    evmc::address m_erc20_addr;

    size_t m_in_flight{};
    bool m_success{false};
    bool m_done{false};
    evmc::uint256be m_current_nonce{};

    cbdc::privkey_t m_skey;
    evmc::uint256be m_total_mint{};
    evmc::address m_init_addr;
    cbdc::privkey_t m_init_skey;

    size_t m_txs{};
    bool m_error{false};

    std::ofstream m_samples_file;

    std::atomic_bool m_running{true};

    size_t m_total_accounts;

    std::optional<std::chrono::high_resolution_clock::time_point> m_start_time;

    auto gen_tx(evmc::uint256be nonce,
                evmc::address to_addr,
                cbdc::privkey_t skey,
                evmc::uint256be value) -> std::string;

    auto deploy_erc20(evmc::uint256be nonce, cbdc::privkey_t skey)
        -> std::string;

    auto send_erc20(evmc::address erc20_addr,
                    evmc::uint256be nonce,
                    evmc::address to_addr,
                    cbdc::privkey_t skey,
                    evmc::uint256be value) -> std::string;

    auto new_account() -> std::pair<cbdc::privkey_t, evmc::address>;

    void mint_one(size_t count,
                  evmc::uint256be mint_amt,
                  cbdc::privkey_t acc_skey,
                  size_t depth);

    void mint_tree(size_t depth, cbdc::privkey_t acc_skey);
};

#endif
