// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "common/config.hpp"
#include "controller.hpp"
#include "crypto/sha256.h"
#include "network/connection_manager.hpp"
#include "serialization/format.hpp"

#include <csignal>

// LCOV_EXCL_START
auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    if(args.size() < 3) {
        std::cerr << "Usage: " << args[0] << " <config file> <watchtower ID>"
                  << std::endl;
        return 0;
    }

    const auto watchtower_id = std::stoull(args[2]);

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        std::cerr << "Error loading config file: "
                  << std::get<std::string>(cfg_or_err) << std::endl;
        return -1;
    }
    auto opts = std::get<cbdc::config::options>(cfg_or_err);

    auto logger = std::make_shared<cbdc::logging::log>(
        opts.m_watchtower_loglevels[watchtower_id]);

    auto ctl
        = cbdc::watchtower::controller{static_cast<uint32_t>(watchtower_id),
                                       opts,
                                       logger};

    if(!ctl.init()) {
        return -1;
    }

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
