// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include <csignal>
#include <unordered_map>

// LCOV_EXCL_START
auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    if(args.size() < 3) {
        std::cerr << "Usage: " << args[0] << " <config file> <sentinel id>"
                  << std::endl;
        return -1;
    }

    const auto sentinel_id = std::stoull(args[2]);

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        std::cerr << "Error loading config file: "
                  << std::get<std::string>(cfg_or_err) << std::endl;
        return -1;
    }
    auto opts = std::get<cbdc::config::options>(cfg_or_err);

    if(opts.m_sentinel_endpoints.size() <= sentinel_id) {
        std::cerr << "Sentinel ID not in config file" << std::endl;
        return -1;
    }

    auto logger = std::make_shared<cbdc::logging::log>(
        opts.m_sentinel_loglevels[sentinel_id]);

    std::string sha2_impl(SHA256AutoDetect());
    logger->info("using sha2:", sha2_impl);

    static std::atomic_bool running{true};

    auto ctl
        = cbdc::sentinel_2pc::controller{static_cast<uint32_t>(sentinel_id),
                                         opts,
                                         logger};
    if(!ctl.init()) {
        return -1;
    }

    // Wait for CTRL+C etc
    std::signal(SIGINT, [](int /* sig */) {
        running = false;
    });

    logger->info("Sentinel running...");

    while(running) {
        static constexpr auto running_check_delay
            = std::chrono::milliseconds(1000);
        std::this_thread::sleep_for(running_check_delay);
    }

    logger->info("Shutting down...");

    ctl.stop();

    return 0;
}
// LCOV_EXCL_STOP
