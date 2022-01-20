// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "common/config.hpp"
#include "controller.hpp"
#include "raft/console_logger.hpp"

#include <csignal>

// LCOV_EXCL_START
auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    if(args.size() < 3) {
        std::cerr << "Usage: " << args[0] << " <config file> <atomizer id>"
                  << std::endl;
        return 0;
    }

    const auto atomizer_id = std::stoull(args[2]);

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        std::cerr << "Error loading config file: "
                  << std::get<std::string>(cfg_or_err) << std::endl;
        return -1;
    }
    auto opts = std::get<cbdc::config::options>(cfg_or_err);

    if(opts.m_atomizer_endpoints.size() <= atomizer_id) {
        std::cerr << "Atomizer ID not in config file" << std::endl;
        return -1;
    }

    auto logger = std::make_shared<cbdc::logging::log>(
        opts.m_atomizer_loglevels[atomizer_id]);

    auto ctl = cbdc::atomizer::controller{static_cast<uint32_t>(atomizer_id),
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

    return 0;
}
// LCOV_EXCL_STOP
