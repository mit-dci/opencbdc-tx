// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util/common/commitment.hpp"
#include "util/common/config.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/util.hpp"
#include "uhs/transaction/validation.hpp"

#include <secp256k1_bppp.h>
#include <unordered_map>

using secp256k1_context_destroy_type = void (*)(secp256k1_context*);

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

    std::unique_ptr<secp256k1_context,
                    secp256k1_context_destroy_type>
        secp{secp256k1_context_create(SECP256K1_CONTEXT_NONE),
             &secp256k1_context_destroy};

    auto expected = (cfg.m_seed_to - cfg.m_seed_from) * cfg.m_seed_value;

    auto audits = std::unordered_map<
        uint64_t,
        std::vector<secp256k1_pedersen_commitment>>();

    for(auto& audit_file : cfg.m_shard_audit_logs) {
        auto f = std::ifstream(audit_file);
        if(!f.good()) {
            log.error("Unable to open audit log");
            return -1;
        }

        uint64_t epoch{};
        std::string commit_hex{};
        while(f >> epoch >> commit_hex) {
            auto commitbuf = cbdc::buffer::from_hex(commit_hex);
            auto comm
                = cbdc::from_buffer<cbdc::commitment_t>(commitbuf.value())
                      .value();
            auto maybe_summary = cbdc::deserialize_commitment(secp.get(), comm);
            assert(maybe_summary.has_value());
            auto summary = maybe_summary.value();

            auto it = audits.find(epoch);
            if(it != audits.end()) {
                auto& summaries = it->second;
                summaries.emplace_back(std::move(summary));
            } else {
                auto summaries
                    = std::vector<secp256k1_pedersen_commitment>();
                summaries.emplace_back(summary);
                audits[epoch] = std::move(summaries);
            }
        }
    }

    // todo: per-epoch: get a vector of all commitments, push_back
    // circulation_commitment, check sum = 1
    for(auto& [epoch, summaries] : audits) {
        auto success =
            cbdc::transaction::validation::check_commitment_sum(summaries, {}, expected);
        std::cout << "epoch " << epoch << ": "
                  << (success ? "PASS" : "FAIL")
                  << " with "
                  << summaries.size() << "/" << cfg.m_shard_audit_logs.size()
                  << " reporting"
                  << std::endl;
    }

    return 0;
}
