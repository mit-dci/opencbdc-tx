// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"
#include "serialization/format.hpp"

#include <csignal>
#include <iostream>

// LCOV_EXCL_START
auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);

    if(args.size() < 3) {
        std::cerr << "Usage: " << args[0]
                  << " <config file> <archiver id> [<max samples>]"
                  << std::endl;
        return 0;
    }

    size_t max_samples{0};
    if(args.size() > 3) {
        max_samples = static_cast<size_t>(std::stoull(args[3]));
    }

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        std::cerr << "Error loading config file: "
                  << std::get<std::string>(cfg_or_err) << std::endl;
        return -1;
    }
    auto opts = std::get<cbdc::config::options>(cfg_or_err);

    const auto archiver_id = std::stoull(args[2]);

    if(opts.m_archiver_endpoints.size() <= archiver_id) {
        std::cerr << "Archiver ID not in config file" << std::endl;
        return -1;
    }

    auto logger = std::make_shared<cbdc::logging::log>(
        opts.m_archiver_loglevels[archiver_id]);

    auto ctl = cbdc::archiver::controller(static_cast<uint32_t>(archiver_id),
                                          opts,
                                          logger,
                                          max_samples);

    if(!ctl.init()) {
        return -1;
    }

    // Can't capture variables in lambda for sighandler.
    // Have to make this static instead.
    static std::atomic_bool running{true};

    // Wait for CTRL+C etc
    std::signal(SIGINT, [](int /* signal */) {
        running = false;
    });

    logger->info("Archiver running...");

    while(running && ctl.running()) {
        static constexpr auto running_check_delay
            = std::chrono::milliseconds(1000);
        std::this_thread::sleep_for(running_check_delay);
    }

    logger->info("Shutting down...");

    return 0;
}
// LCOV_EXCL_STOP
