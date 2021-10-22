// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "common/config.hpp"

#include <unordered_map>

auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    if(args.size() < 2) {
        std::cout << "Usage: " << args[0] << " [config file]" << std::endl;
        return -1;
    }

    auto log = cbdc::logging::log(cbdc::logging::log_level::trace);

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        log.error("Error loading config file:",
                  std::get<std::string>(cfg_or_err));
        return -1;
    }
    auto cfg = std::get<cbdc::config::options>(cfg_or_err);

    struct total {
        uint64_t m_total_value{};
        size_t m_shard_count{};
    };

    auto totals = std::unordered_map<uint64_t, total>();

    for(auto& audit_file : cfg.m_shard_audit_logs) {
        auto f = std::ifstream(audit_file);
        if(!f.good()) {
            log.error("Unable to open audit log");
            return -1;
        }

        uint64_t epoch{};
        uint64_t total_value{};
        while(f >> epoch >> total_value) {
            auto it = totals.find(epoch);
            if(it != totals.end()) {
                it->second.m_total_value += total_value;
                it->second.m_shard_count++;
            } else {
                totals[epoch] = total{total_value, 1};
            }
        }
    }

    for(auto& [epoch, tot] : totals) {
        std::cout << "epoch: " << epoch
                  << ", total_value: " << tot.m_total_value
                  << ", shard_count: " << tot.m_shard_count << std::endl;
    }

    return 0;
}
