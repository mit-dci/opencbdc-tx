// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"
#include "util/common/config.hpp"

#include <cassert>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

// LCOV_EXCL_START
auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    if(args.size() < 3) {
        std::cerr << "Usage: " << args[0] << " <config file> <shard id>"
                  << std::endl;
        return 0;
    }

    const auto shard_id = std::stoull(args[2]);

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        std::cerr << "Error loading config file: "
                  << std::get<std::string>(cfg_or_err) << std::endl;
        return -1;
    }
    auto opts = std::get<cbdc::config::options>(cfg_or_err);

    if(opts.m_shard_endpoints.size() <= shard_id) {
        std::cerr << "Shard ID not in config file" << std::endl;
        return -1;
    }

    auto logger = std::make_shared<cbdc::logging::log>(
        opts.m_shard_loglevels[shard_id]);

    auto ctl = cbdc::shard::controller{static_cast<uint32_t>(shard_id),
                                       opts,
                                       logger};
    if(!ctl.init()) {
        return -1;
    }

    // Wait for CTRL+C etc
    static std::atomic_bool running{true};

    std::signal(SIGINT, [](int /* signal */) {
        running = false;
    });

    while(running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    };

    logger->info("Shutting down...");

    return 0;
}
// LCOV_EXCL_STOP
