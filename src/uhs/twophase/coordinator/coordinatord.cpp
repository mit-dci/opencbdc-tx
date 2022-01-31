// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"
#include "util/common/config.hpp"

#include <csignal>
#include <iostream>

// LCOV_EXCL_START
auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    if(args.size() < 4) {
        std::cout << "Usage: " << args[0]
                  << " <config file> <coordinator ID> <node ID>" << std::endl;
        return 0;
    }

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        std::cerr << "Error loading config file: "
                  << std::get<std::string>(cfg_or_err) << std::endl;
        return -1;
    }
    auto opts = std::get<cbdc::config::options>(cfg_or_err);

    auto coordinator_id = std::stoull(args[2]);
    auto node_id = std::stoull(args[3]);

    if(opts.m_coordinator_endpoints.size() <= coordinator_id) {
        std::cerr << "Coordinator ID not configured" << std::endl;
        return -1;
    }

    if(opts.m_coordinator_endpoints[coordinator_id].size() <= node_id) {
        std::cerr << "Coordinator node ID not configured" << std::endl;
        return -1;
    }

    auto logger = std::make_shared<cbdc::logging::log>(
        opts.m_coordinator_loglevels[coordinator_id]);

    std::string sha2_impl(SHA256AutoDetect());
    logger->info("using sha2: ", sha2_impl);

    auto coord
        = cbdc::coordinator::controller(node_id, coordinator_id, opts, logger);

    if(!coord.init()) {
        logger->fatal("Failed to initialize raft cluster");
    }

    static std::atomic_bool running{true};

    // Wait for CTRL+C etc
    std::signal(SIGINT, [](int /* sig */) {
        running = false;
    });

    logger->info("Coordinator running...");

    while(running) {
        static constexpr auto running_check_delay
            = std::chrono::milliseconds(1000);
        std::this_thread::sleep_for(running_check_delay);
    }

    logger->info("Shutting down...");

    coord.quit();

    return 0;
}
// LCOV_EXCL_STOP
