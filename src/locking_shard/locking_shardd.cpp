// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "common/config.hpp"
#include "controller.hpp"
#include "crypto/sha256.h"

#include <csignal>
#include <iostream>

// LCOV_EXCL_START
auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    if(args.size() < 4) {
        std::cout << "Usage: " << args[0]
                  << " <config file> <shard ID> <node ID>" << std::endl;
        return 0;
    }

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        std::cerr << "Error loading config file: "
                  << std::get<std::string>(cfg_or_err) << std::endl;
        return -1;
    }
    auto cfg = std::get<cbdc::config::options>(cfg_or_err);
    auto shard_id = std::stoull(args[2]);
    auto node_id = std::stoull(args[3]);

    if(cfg.m_locking_shard_endpoints.size() <= shard_id) {
        std::cerr << "Shard ID not in config file" << std::endl;
        return -1;
    }

    if(cfg.m_locking_shard_endpoints[shard_id].size() <= node_id) {
        std::cerr << "Shard node ID not in config file" << std::endl;
        return -1;
    }

    auto logger = std::make_shared<cbdc::logging::log>(
        cfg.m_shard_loglevels[shard_id]);

    std::string sha2_impl(SHA256AutoDetect());
    logger->info("using sha2: ", sha2_impl);

    auto ctl = cbdc::locking_shard::controller(shard_id, node_id, cfg, logger);
    if(!ctl.init()) {
        logger->error("Failed to initialize locking shard");
        return -1;
    }

    static std::atomic_bool running{true};

    // Wait for CTRL+C etc
    std::signal(SIGINT, [](int /* sig */) {
        running = false;
    });

    logger->info("Shard running...");

    while(running) {
        static constexpr auto running_check_delay
            = std::chrono::milliseconds(1000);
        std::this_thread::sleep_for(running_check_delay);
    }

    logger->info("Shutting down...");

    return 0;
}
// LCOV_EXCL_STOP
