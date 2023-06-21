// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evm_bench.hpp"

#include <csignal>

auto main(int argc, char** argv) -> int {
    auto cfg = cbdc::parsec::read_config(argc, argv);
    if(!cfg.has_value()) {
        std::cout << "Error parsing options" << std::endl;
        return 1;
    }

    auto log = std::make_shared<cbdc::logging::log>(cfg->m_loglevel);

    auto endpoints = std::vector<std::string>();
    for(auto& [host, port] : cfg->m_agent_endpoints) {
        auto url = "http://" + host + ":" + std::to_string(port);
        endpoints.push_back(url);
    }

    auto c = std::make_shared<geth_client>(endpoints, 0, log);

    // Determine mint depth from loadgen_accounts
    size_t mint_tree_depth = 1;
    while(static_cast<size_t>(1 << mint_tree_depth)
          < cfg->m_loadgen_accounts) {
        mint_tree_depth++;
    }

    static auto bench
        = evm_bench(cfg->m_component_id, mint_tree_depth, cfg.value(), log, c);

    std::signal(SIGINT, [](int /* sig */) {
        bench.stop();
    });

    bench.deploy();

    auto success = std::optional<bool>();
    while(!success.has_value()) {
        success = bench.pump();
    }

    if(!success.value()) {
        log->error("Could not deploy contract/make initial TX");
        return 2;
    }

    bench.mint_tree(mint_tree_depth - 1);

    success = std::optional<bool>();
    while(!success.has_value()) {
        success = bench.pump();
    }

    if(!success.value()) {
        log->error("Error during minting");
        return 3;
    }

    log->info("Minted", bench.account_count(), "new accounts");

    for(size_t i = 1; i < bench.account_count() - 1; i += 2) {
        bench.schedule_tx(i, i + 1);
    }
    log->flush();

    success = std::optional<bool>();
    while(!success.has_value()) {
        success = bench.pump();
    }

    if(!success.value()) {
        log->error("Error during load generation");
        return 5;
    }

    return 0;
}
