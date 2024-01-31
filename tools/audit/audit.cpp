// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util/common/commitment.hpp"
#include "util/common/config.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/util.hpp"

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

    auto audits = std::unordered_map<
        uint64_t,
        std::unordered_map<unsigned char, cbdc::commitment_t>>();

    // todo: ensure/detect whether or not the most recent audit has finished
    for(auto& audit_file : cfg.m_shard_audit_logs) {
        auto f = std::ifstream(audit_file);
        if(!f.good()) {
            log.error("Unable to open audit log");
            return -1;
        }

        uint64_t epoch{};
        std::string bucket_str{};
        std::string commit_hex{};
        while(f >> epoch >> bucket_str >> commit_hex) {
            auto bucket = static_cast<unsigned char>(std::stoul(bucket_str));

            auto commitbuf = cbdc::buffer::from_hex(commit_hex);
            auto commit
                = cbdc::from_buffer<cbdc::commitment_t>(commitbuf.value())
                      .value();

            auto it = audits.find(epoch);
            if(it != audits.end()) {
                auto& audit = it->second;
                auto entry = audit.find(bucket);
                if(entry != audit.end()) {
                    if(entry->second != commit) {
                        std::cerr << "Audit failed at epoch " << epoch
                                  << "; inconsistency in range " << bucket_str
                                  << std::endl;
                        return 1;
                    }
                } else {
                    audit[bucket] = commit;
                }
            } else {
                auto entries
                    = std::unordered_map<unsigned char, cbdc::commitment_t>();
                entries.emplace(bucket, commit);
                audits[epoch] = std::move(entries);
            }
        }
    }

    // todo: per-epoch: get a vector of all commitments, push_back
    // circulation_commitment, check sum = 1
    for(auto& [epoch, entries] : audits) {
        std::cout << "epoch: " << epoch << std::endl;
    }

    return 0;
}
