// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

namespace cbdc::config {
    auto parse_ip_port(const std::string& in_str) -> network::endpoint_t {
        // TODO: error handling for string parsing
        std::istringstream ss(in_str);

        std::string host;
        [[maybe_unused]] const auto& host_res = std::getline(ss, host, ':');
        assert(host_res);

        std::string port_str;
        [[maybe_unused]] const auto& port_res
            = std::getline(ss, port_str, ':');
        assert(port_res);

        auto port = std::stoul(port_str);
        assert(port <= std::numeric_limits<unsigned short>::max());

        return {host, static_cast<unsigned short>(port)};
    }

    void get_shard_key_prefix(std::stringstream& ss, size_t shard_id) {
        ss << shard_prefix << shard_id << config_separator;
    }

    auto get_shard_endpoint_key(size_t shard_id) -> std::string {
        std::stringstream ss;
        get_shard_key_prefix(ss, shard_id);
        ss << endpoint_postfix;
        return ss.str();
    }

    auto get_atomizer_endpoint_key(size_t atomizer_id) -> std::string {
        std::stringstream ss;
        ss << atomizer_prefix << atomizer_id << config_separator
           << endpoint_postfix;
        return ss.str();
    }

    auto get_atomizer_raft_endpoint_key(size_t atomizer_id) -> std::string {
        std::stringstream ss;
        ss << atomizer_prefix << atomizer_id << config_separator
           << raft_endpoint_postfix;
        return ss.str();
    }

    auto get_atomizer_loglevel_key(size_t atomizer_id) -> std::string {
        std::stringstream ss;
        ss << atomizer_prefix << atomizer_id << config_separator
           << loglevel_postfix;
        return ss.str();
    }

    auto get_sentinel_endpoint_key(size_t sentinel_id) -> std::string {
        std::stringstream ss;
        ss << sentinel_prefix << sentinel_id << config_separator
           << endpoint_postfix;
        return ss.str();
    }

    auto get_shard_db_key(size_t shard_id) -> std::string {
        std::stringstream ss;
        get_shard_key_prefix(ss, shard_id);
        ss << db_postfix;
        return ss.str();
    }

    auto get_shard_end_key(size_t shard_id) -> std::string {
        std::stringstream ss;
        get_shard_key_prefix(ss, shard_id);
        ss << end_postfix;
        return ss.str();
    }

    auto get_shard_start_key(size_t shard_id) -> std::string {
        std::stringstream ss;
        get_shard_key_prefix(ss, shard_id);
        ss << start_postfix;
        return ss.str();
    }

    void get_archiver_key_prefix(std::stringstream& ss, size_t archiver_id) {
        ss << archiver_prefix << archiver_id << config_separator;
    }

    auto get_archiver_endpoint_key(size_t archiver_id) -> std::string {
        std::stringstream ss;
        get_archiver_key_prefix(ss, archiver_id);
        ss << endpoint_postfix;
        return ss.str();
    }

    auto get_archiver_loglevel_key(size_t archiver_id) -> std::string {
        std::stringstream ss;
        get_archiver_key_prefix(ss, archiver_id);
        ss << loglevel_postfix;
        return ss.str();
    }

    auto get_archiver_db_key(size_t archiver_id) -> std::string {
        std::stringstream ss;
        get_archiver_key_prefix(ss, archiver_id);
        ss << db_postfix;
        return ss.str();
    }

    auto get_shard_loglevel_key(size_t shard_id) -> std::string {
        std::stringstream ss;
        get_shard_key_prefix(ss, shard_id);
        ss << loglevel_postfix;
        return ss.str();
    }

    void get_sentinel_key_prefix(std::stringstream& ss, size_t sentinel_id) {
        ss << sentinel_prefix << sentinel_id << config_separator;
    }

    auto get_sentinel_loglevel_key(size_t sentinel_id) -> std::string {
        std::stringstream ss;
        get_sentinel_key_prefix(ss, sentinel_id);
        ss << loglevel_postfix;
        return ss.str();
    }

    void get_watchtower_key_prefix(std::stringstream& ss,
                                   size_t watchtower_id) {
        ss << watchtower_prefix << watchtower_id << config_separator;
    }

    auto get_watchtower_client_endpoint_key(size_t watchtower_id)
        -> std::string {
        std::stringstream ss;
        get_watchtower_key_prefix(ss, watchtower_id);
        ss << watchtower_client_ep_postfix;
        return ss.str();
    }

    auto get_watchtower_internal_endpoint_key(size_t watchtower_id)
        -> std::string {
        std::stringstream ss;
        get_watchtower_key_prefix(ss, watchtower_id);
        ss << watchtower_internal_ep_postfix;
        return ss.str();
    }

    auto get_watchtower_loglevel_key(size_t watchtower_id) -> std::string {
        std::stringstream ss;
        get_watchtower_key_prefix(ss, watchtower_id);
        ss << loglevel_postfix;
        return ss.str();
    }

    auto get_shard_node_count_key(size_t shard_id) -> std::string {
        std::stringstream ss;
        get_shard_key_prefix(ss, shard_id);
        ss << count_postfix;
        return ss.str();
    }

    auto get_shard_raft_endpoint_key(size_t shard_id, size_t node_id)
        -> std::string {
        std::stringstream ss;
        get_shard_key_prefix(ss, shard_id);
        ss << node_id << config_separator << raft_endpoint_postfix;
        return ss.str();
    }

    auto get_shard_endpoint_key(size_t shard_id, size_t node_id)
        -> std::string {
        std::stringstream ss;
        get_shard_key_prefix(ss, shard_id);
        ss << node_id << config_separator << endpoint_postfix;
        return ss.str();
    }

    auto get_shard_readonly_endpoint_key(size_t shard_id, size_t node_id)
        -> std::string {
        std::stringstream ss;
        get_shard_key_prefix(ss, shard_id);
        ss << node_id << config_separator << readonly << config_separator
           << endpoint_postfix;
        return ss.str();
    }

    void get_coordinator_key_prefix(std::stringstream& ss,
                                    size_t coordinator_id) {
        ss << coordinator_prefix << coordinator_id << config_separator;
    }

    auto get_coordinator_endpoint_key(size_t coordinator_id, size_t node_id)
        -> std::string {
        std::stringstream ss;
        get_coordinator_key_prefix(ss, coordinator_id);
        ss << node_id << config_separator << endpoint_postfix;
        return ss.str();
    }

    auto get_coordinator_raft_endpoint_key(size_t coordinator_id,
                                           size_t node_id) -> std::string {
        std::stringstream ss;
        get_coordinator_key_prefix(ss, coordinator_id);
        ss << node_id << config_separator << raft_endpoint_postfix;
        return ss.str();
    }

    auto get_coordinator_node_count_key(size_t coordinator_id) -> std::string {
        std::stringstream ss;
        get_coordinator_key_prefix(ss, coordinator_id);
        ss << count_postfix;
        return ss.str();
    }

    auto get_coordinator_loglevel_key(size_t coordinator_id) -> std::string {
        std::stringstream ss;
        ss << coordinator_prefix << coordinator_id << config_separator
           << loglevel_postfix;
        return ss.str();
    }

    auto get_loadgen_loglevel_key(size_t loadgen_id) -> std::string {
        std::stringstream ss;
        ss << loadgen_prefix << loadgen_id << config_separator
           << loglevel_postfix;
        return ss.str();
    }

    auto get_sentinel_private_key_key(size_t sentinel_id) -> std::string {
        auto ss = std::stringstream();
        get_sentinel_key_prefix(ss, sentinel_id);
        ss << private_key_postfix;
        return ss.str();
    }

    auto get_sentinel_public_key_key(size_t sentinel_id) -> std::string {
        auto ss = std::stringstream();
        get_sentinel_key_prefix(ss, sentinel_id);
        ss << public_key_postfix;
        return ss.str();
    }

    auto read_shard_endpoints(options& opts, const parser& cfg)
        -> std::optional<std::string> {
        const auto shard_count = cfg.get_ulong(shard_count_key).value_or(0);
        if(opts.m_twophase_mode) {
            opts.m_locking_shard_endpoints.resize(shard_count);
            opts.m_locking_shard_raft_endpoints.resize(shard_count);
            opts.m_locking_shard_readonly_endpoints.resize(shard_count);
            for(size_t i{0}; i < shard_count; i++) {
                const auto node_count_key = get_shard_node_count_key(i);
                const auto node_count = cfg.get_ulong(node_count_key);
                if(!node_count) {
                    return "No node count specified for shard "
                         + std::to_string(i) + " (" + node_count_key + ")";
                }
                for(size_t j{0}; j < *node_count; j++) {
                    const auto raft_ep_key = get_shard_raft_endpoint_key(i, j);
                    const auto raft_ep = cfg.get_endpoint(raft_ep_key);
                    if(!raft_ep) {
                        return "No raft endpoint specified for shard "
                             + std::to_string(i) + " node " + std::to_string(j)
                             + " (" + raft_ep_key + ")";
                    }
                    opts.m_locking_shard_raft_endpoints[i].emplace_back(
                        *raft_ep);

                    const auto ep_key = get_shard_endpoint_key(i, j);
                    const auto ep = cfg.get_endpoint(ep_key);
                    if(!ep) {
                        return "No endpoint specified for shard "
                             + std::to_string(i) + " node "
                             + std::to_string(j);
                    }
                    opts.m_locking_shard_endpoints[i].emplace_back(*ep);

                    const auto ro_ep_key
                        = get_shard_readonly_endpoint_key(i, j);
                    const auto ro_ep = cfg.get_endpoint(ro_ep_key);
                    if(!ro_ep) {
                        return "No read-only endpoint specified for shard "
                             + std::to_string(i) + " node " + std::to_string(j)
                             + " (" + ro_ep_key + ")";
                    }
                    opts.m_locking_shard_readonly_endpoints[i].emplace_back(
                        *ro_ep);
                }
            }
        }

        return std::nullopt;
    }

    auto read_shard_options(options& opts, const parser& cfg)
        -> std::optional<std::string> {
        const auto shard_count = cfg.get_ulong(shard_count_key).value_or(0);
        for(size_t i{0}; i < shard_count; i++) {
            if(!opts.m_twophase_mode) {
                const auto shard_ep_key = get_shard_endpoint_key(i);
                const auto shard_ep = cfg.get_endpoint(shard_ep_key);
                if(!shard_ep) {
                    return "No endpoint specified for shard "
                         + std::to_string(i) + " (" + shard_ep_key + ")";
                }
                opts.m_shard_endpoints.push_back(*shard_ep);

                const auto shard_db_str = get_shard_db_key(i);
                const auto shard_db = cfg.get_string(shard_db_str);
                if(!shard_db) {
                    return "No db directory specified for shard "
                         + std::to_string(i) + " (" + shard_db_str + ")";
                }
                opts.m_shard_db_dirs.push_back(*shard_db);
            }

            const auto shard_loglevel_key = get_shard_loglevel_key(i);
            const auto shard_loglevel = cfg.get_loglevel(shard_loglevel_key)
                                            .value_or(defaults::log_level);
            opts.m_shard_loglevels.push_back(shard_loglevel);

            const auto start_key = get_shard_start_key(i);
            const auto range_start = cfg.get_ulong(start_key);
            if(!range_start) {
                return "No range start specified for shard "
                     + std::to_string(i) + " (" + start_key + ")";
            }
            const auto end_key = get_shard_end_key(i);
            const auto range_end = cfg.get_ulong(end_key);
            if(!range_end) {
                return "No range end specified for shard " + std::to_string(i)
                     + " (" + end_key + ")";
            }
            const auto shard_range
                = std::make_pair(static_cast<uint8_t>(*range_start),
                                 static_cast<uint8_t>(*range_end));
            opts.m_shard_ranges.push_back(shard_range);
        }

        opts.m_shard_completed_txs_cache_size
            = cfg.get_ulong(shard_completed_txs_cache_size)
                  .value_or(opts.m_shard_completed_txs_cache_size);

        opts.m_seed_from = cfg.get_ulong(seed_from).value_or(opts.m_seed_from);
        opts.m_seed_to = cfg.get_ulong(seed_to).value_or(opts.m_seed_to);
        if(opts.m_seed_from != opts.m_seed_to) {
            auto priv_str = cfg.get_string(seed_privkey);
            if(!priv_str) {
                return "Seed range defined but missing a private key";
            }
            if(priv_str.value().size() != sizeof(privkey_t) * 2) {
                return "Invalid seed private key length";
            }
            opts.m_seed_privkey = hash_from_hex(priv_str.value());
            opts.m_seed_value
                = cfg.get_ulong(seed_value).value_or(opts.m_seed_value);
        }

        return std::nullopt;
    }

    auto read_coordinator_options(options& opts, const parser& cfg)
        -> std::optional<std::string> {
        const auto coordinator_count
            = cfg.get_ulong(coordinator_count_key).value_or(0);
        opts.m_coordinator_endpoints.resize(coordinator_count);
        opts.m_coordinator_raft_endpoints.resize(coordinator_count);
        for(size_t i{0}; i < coordinator_count; i++) {
            const auto loglevel_key = get_coordinator_loglevel_key(i);
            const auto coordinator_loglevel
                = cfg.get_loglevel(loglevel_key).value_or(defaults::log_level);
            opts.m_coordinator_loglevels.push_back(coordinator_loglevel);

            const auto node_count_key = get_coordinator_node_count_key(i);
            const auto node_count = cfg.get_ulong(node_count_key);
            if(!node_count) {
                return "No node count specified for coordinator "
                     + std::to_string(i) + " (" + node_count_key + ")";
            }
            for(size_t j{0}; j < *node_count; j++) {
                const auto raft_ep_key
                    = get_coordinator_raft_endpoint_key(i, j);
                const auto raft_ep = cfg.get_endpoint(raft_ep_key);
                if(!raft_ep) {
                    return "No raft endpoint specified for coordinator "
                         + std::to_string(i) + " node " + std::to_string(j)
                         + " (" + raft_ep_key + ")";
                }
                opts.m_coordinator_raft_endpoints[i].emplace_back(*raft_ep);

                const auto ep_key = get_coordinator_endpoint_key(i, j);
                const auto ep = cfg.get_endpoint(ep_key);
                if(!ep) {
                    return "No endpoint specified for coordinator "
                         + std::to_string(i) + " node " + std::to_string(j)
                         + " (" + ep_key + ")";
                }
                opts.m_coordinator_endpoints[i].emplace_back(*ep);
            }
        }

        opts.m_coordinator_max_threads
            = cfg.get_ulong(coordinator_max_threads)
                  .value_or(opts.m_coordinator_max_threads);

        return std::nullopt;
    }

    auto read_sentinel_options(options& opts, const parser& cfg)
        -> std::optional<std::string> {
        opts.m_attestation_threshold
            = cfg.get_ulong(attestation_threshold_key)
                  .value_or(opts.m_attestation_threshold);

        const auto sentinel_count
            = cfg.get_ulong(sentinel_count_key).value_or(0);
        for(size_t i{0}; i < sentinel_count; i++) {
            const auto sentinel_ep_key = get_sentinel_endpoint_key(i);
            const auto sentinel_ep = cfg.get_endpoint(sentinel_ep_key);
            if(!sentinel_ep) {
                return "No endpoint specified for sentinel "
                     + std::to_string(i) + " (" + sentinel_ep_key + ")";
            }
            opts.m_sentinel_endpoints.push_back(*sentinel_ep);

            const auto sentinel_loglevel_key = get_sentinel_loglevel_key(i);
            const auto sentinel_loglevel
                = cfg.get_loglevel(sentinel_loglevel_key)
                      .value_or(defaults::log_level);
            opts.m_sentinel_loglevels.push_back(sentinel_loglevel);

            const auto sentinel_private_key_key
                = get_sentinel_private_key_key(i);
            const auto sentinel_private_key
                = cfg.get_string(sentinel_private_key_key);
            if(sentinel_private_key.has_value()) {
                auto key = hash_from_hex(sentinel_private_key.value());
                opts.m_sentinel_private_keys[i] = key;
            }

            const auto sentinel_public_key_key
                = get_sentinel_public_key_key(i);
            const auto sentinel_public_key
                = cfg.get_string(sentinel_public_key_key);
            if(!sentinel_public_key.has_value()) {
                if(opts.m_attestation_threshold == 0) {
                    continue;
                }
                return "No public key specified for sentinel "
                     + std::to_string(i) + " (" + sentinel_public_key_key
                     + ")";
            }
            auto key = hash_from_hex(sentinel_public_key.value());
            opts.m_sentinel_public_keys.insert(key);
        }
        return std::nullopt;
    }

    auto read_atomizer_options(options& opts, const parser& cfg)
        -> std::optional<std::string> {
        const auto atomizer_count
            = cfg.get_ulong(atomizer_count_key).value_or(0);
        for(size_t i{0}; i < atomizer_count; i++) {
            const auto atomizer_ep_key = get_atomizer_endpoint_key(i);
            const auto atomizer_ep = cfg.get_endpoint(atomizer_ep_key);
            if(!atomizer_ep) {
                return "No endpoint specified for atomizer "
                     + std::to_string(i) + " (" + atomizer_ep_key + ")";
            }
            opts.m_atomizer_endpoints.push_back(*atomizer_ep);

            const auto atomizer_loglevel_key = get_atomizer_loglevel_key(i);
            const auto atomizer_loglevel
                = cfg.get_loglevel(atomizer_loglevel_key);
            const auto loglevel
                = atomizer_loglevel.value_or(defaults::log_level);
            opts.m_atomizer_loglevels.push_back(loglevel);

            const auto endpoint_key = get_atomizer_raft_endpoint_key(i);
            const auto endpoint_str = cfg.get_endpoint(endpoint_key);
            if(!endpoint_str) {
                return "No raft endpoint specified for atomizer "
                     + std::to_string(i) + " (" + endpoint_key + ")";
            }
            opts.m_atomizer_raft_endpoints.push_back(endpoint_str.value());
        }

        opts.m_target_block_interval
            = cfg.get_ulong(target_block_interval_key)
                  .value_or(opts.m_target_block_interval);

        opts.m_stxo_cache_depth
            = cfg.get_ulong(stxo_cache_key).value_or(opts.m_stxo_cache_depth);

        return std::nullopt;
    }

    auto read_archiver_options(options& opts, const parser& cfg)
        -> std::optional<std::string> {
        const auto archiver_count
            = cfg.get_ulong(archiver_count_key).value_or(0);
        for(size_t i{0}; i < archiver_count; i++) {
            const auto archiver_ep_key = get_archiver_endpoint_key(i);
            const auto archiver_ep = cfg.get_endpoint(archiver_ep_key);
            if(!archiver_ep) {
                return "No endpoint specified for archiver "
                     + std::to_string(i) + " (" + archiver_ep_key + ")";
            }
            opts.m_archiver_endpoints.push_back(*archiver_ep);

            const auto archiver_loglevel_key = get_archiver_loglevel_key(i);
            const auto archiver_loglevel
                = cfg.get_loglevel(archiver_loglevel_key)
                      .value_or(defaults::log_level);
            opts.m_archiver_loglevels.push_back(archiver_loglevel);

            const auto archiver_db_str = get_archiver_db_key(i);
            const auto archiver_db = cfg.get_string(archiver_db_str);
            if(!archiver_db) {
                return "No db directory specified for archiver "
                     + std::to_string(i) + " (" + archiver_db_str + ")";
            }
            opts.m_archiver_db_dirs.push_back(*archiver_db);
        }

        return std::nullopt;
    }

    auto read_watchtower_options(options& opts, const parser& cfg)
        -> std::optional<std::string> {
        const auto watchtower_count
            = cfg.get_ulong(watchtower_count_key).value_or(0);
        for(size_t i{0}; i < watchtower_count; i++) {
            const auto watchtower_client_ep_key
                = get_watchtower_client_endpoint_key(i);
            const auto watchtower_client_ep
                = cfg.get_endpoint(watchtower_client_ep_key);
            if(!watchtower_client_ep) {
                return "No client endpoint specified for watchtower "
                     + std::to_string(i) + " (" + watchtower_client_ep_key
                     + ")";
            }
            opts.m_watchtower_client_endpoints.push_back(
                *watchtower_client_ep);

            const auto watchtower_internal_ep_key
                = get_watchtower_internal_endpoint_key(i);
            const auto watchtower_internal_ep
                = cfg.get_endpoint(watchtower_internal_ep_key);
            if(!watchtower_internal_ep) {
                return "No internal endpoint specified for watchtower "
                     + std::to_string(i) + std::to_string(i) + " ("
                     + watchtower_internal_ep_key + ")";
            }
            opts.m_watchtower_internal_endpoints.push_back(
                *watchtower_internal_ep);

            const auto watchtower_loglevel_key
                = get_watchtower_loglevel_key(i);
            const auto watchtower_loglevel
                = cfg.get_loglevel(watchtower_loglevel_key)
                      .value_or(defaults::log_level);
            opts.m_watchtower_loglevels.push_back(watchtower_loglevel);
        }

        opts.m_watchtower_block_cache_size
            = cfg.get_ulong(watchtower_block_cache_size_key)
                  .value_or(opts.m_watchtower_block_cache_size);
        opts.m_watchtower_error_cache_size
            = cfg.get_ulong(watchtower_error_cache_size_key)
                  .value_or(opts.m_watchtower_error_cache_size);

        return std::nullopt;
    }

    void read_raft_options(options& opts, const parser& cfg) {
        opts.m_election_timeout_upper = static_cast<int32_t>(
            cfg.get_ulong(election_timeout_upper_key)
                .value_or(opts.m_election_timeout_upper));
        opts.m_election_timeout_lower = static_cast<int32_t>(
            cfg.get_ulong(election_timeout_lower_key)
                .value_or(opts.m_election_timeout_lower));
        opts.m_heartbeat = static_cast<int32_t>(
            cfg.get_ulong(heartbeat_key).value_or(opts.m_heartbeat));
        opts.m_snapshot_distance
            = static_cast<int32_t>(cfg.get_ulong(snapshot_distance_key)
                                       .value_or(opts.m_snapshot_distance));
        opts.m_raft_max_batch
            = static_cast<int32_t>(cfg.get_ulong(raft_batch_size_key)
                                       .value_or(opts.m_raft_max_batch));

        opts.m_batch_size
            = cfg.get_ulong(batch_size_key).value_or(opts.m_batch_size);
    }

    void read_loadgen_options(options& opts, const parser& cfg) {
        opts.m_input_count
            = cfg.get_ulong(input_count_key).value_or(opts.m_input_count);
        opts.m_output_count
            = cfg.get_ulong(output_count_key).value_or(opts.m_output_count);
        opts.m_invalid_rate
            = cfg.get_decimal(invalid_rate_key).value_or(opts.m_invalid_rate);
        opts.m_fixed_tx_rate = cfg.get_decimal(fixed_tx_rate_key)
                                   .value_or(opts.m_fixed_tx_rate);
        opts.m_fixed_tx_mode
            = opts.m_input_count != 0 && opts.m_output_count != 0;
        opts.m_window_size
            = cfg.get_ulong(window_size_key).value_or(opts.m_window_size);

        opts.m_initial_mint_count = cfg.get_ulong(initial_mint_count_key)
                                        .value_or(opts.m_initial_mint_count);
        opts.m_initial_mint_value = cfg.get_ulong(initial_mint_value_key)
                                        .value_or(opts.m_initial_mint_value);

        opts.m_loadgen_count
            = cfg.get_ulong(loadgen_count_key).value_or(opts.m_loadgen_count);
        opts.m_loadgen_tps_target = cfg.get_ulong(tps_target_key)
                                        .value_or(opts.m_loadgen_tps_target);
        opts.m_loadgen_tps_step_time
            = cfg.get_decimal(tps_steptime_key)
                  .value_or(opts.m_loadgen_tps_step_time);
        opts.m_loadgen_tps_step_size
            = cfg.get_decimal(tps_stepsize_key)
                  .value_or(opts.m_loadgen_tps_step_size);
        opts.m_loadgen_tps_initial = cfg.get_decimal(tps_initial_key)
                                         .value_or(opts.m_loadgen_tps_initial);
        for(size_t i{0}; i < opts.m_loadgen_count; ++i) {
            const auto loadgen_loglevel_key = get_loadgen_loglevel_key(i);
            const auto loadgen_loglevel
                = cfg.get_loglevel(loadgen_loglevel_key)
                      .value_or(defaults::log_level);
            opts.m_loadgen_loglevels.push_back(loadgen_loglevel);
        }
    }

    auto read_options(const std::string& config_file)
        -> std::variant<options, std::string> {
        auto opts = options{};
        auto cfg = parser(config_file);

        opts.m_twophase_mode = cfg.get_ulong(two_phase_mode).value_or(0) != 0;

        auto err = read_sentinel_options(opts, cfg);
        if(err.has_value()) {
            return err.value();
        }

        err = read_shard_endpoints(opts, cfg);
        if(err.has_value()) {
            return err.value();
        }

        err = read_shard_options(opts, cfg);
        if(err.has_value()) {
            return err.value();
        }

        err = read_atomizer_options(opts, cfg);
        if(err.has_value()) {
            return err.value();
        }

        err = read_archiver_options(opts, cfg);
        if(err.has_value()) {
            return err.value();
        }

        err = read_watchtower_options(opts, cfg);
        if(err.has_value()) {
            return err.value();
        }

        err = read_coordinator_options(opts, cfg);
        if(err.has_value()) {
            return err.value();
        }

        read_raft_options(opts, cfg);

        read_loadgen_options(opts, cfg);

        return opts;
    }

    auto load_options(const std::string& config_file)
        -> std::variant<options, std::string> {
        auto opt = read_options(config_file);
        if(std::holds_alternative<options>(opt)) {
            auto res = check_options(std::get<options>(opt));
            if(res) {
                return *res;
            }
        }
        return opt;
    }

    auto check_options(const options& opts) -> std::optional<std::string> {
        // TODO: refactor options to use maps rather than vectors and move all
        //       config checking to this function.
        if(opts.m_twophase_mode) {
            if(opts.m_sentinel_endpoints.empty()) {
                return "Two-phase mode requires at least one configured "
                       "sentinel";
            }
            if(opts.m_sentinel_endpoints.size()
               < opts.m_attestation_threshold) {
                return "The number of required attestations is larger \n"
                       "than the number of sentinels that can provide them.";
            }
            if(opts.m_locking_shard_endpoints.empty()) {
                return "Two-phase mode requires at least one configured shard";
            }
            if(opts.m_coordinator_endpoints.empty()) {
                return "Two-phase mode required at least one configured "
                       "coordinator";
            }
        } else {
            if(opts.m_watchtower_client_endpoints.empty()) {
                return "Atomizer mode requires at least one configured "
                       "watchtower";
            }
            if(opts.m_archiver_endpoints.empty()) {
                return "Atomizer mode requires at least one configured "
                       "archiver";
            }
            if(opts.m_shard_endpoints.empty()
               && !opts.m_sentinel_endpoints.empty()) {
                return "Sentinels require at least one configured shard";
            }
            if(opts.m_atomizer_endpoints.empty()) {
                return "Atomizer mode requires at least one configured "
                       "atomizer";
            }
        }

        if(opts.m_seed_from != opts.m_seed_to) {
            if(opts.m_seed_from > opts.m_seed_to) {
                return "shard_seed_from > shard_seed_to";
            }
            if(opts.m_seed_value == 0) {
                return "Seed range defined but value is zero";
            }
        }

        if(opts.m_sentinel_public_keys.size() < opts.m_attestation_threshold) {
            return "Not enough sentinel public keys to reach the attestation "
                   "threshold";
        }

        return std::nullopt;
    }

    auto hash_in_shard_range(const shard_range_t& range, const hash_t& val)
        -> bool {
        return val[0] >= range.first && val[0] <= range.second;
    }

    auto loadgen_seed_range(const options& opts, size_t gen_id)
        -> std::pair<size_t, size_t> {
        assert(gen_id < opts.m_loadgen_count);
        auto total_seed_range = opts.m_seed_to - opts.m_seed_from;
        auto seed_range_sz = total_seed_range / opts.m_loadgen_count;
        auto our_range_start = opts.m_seed_from + (gen_id * seed_range_sz);
        auto our_range_end = our_range_start + seed_range_sz - 1;
        return {our_range_start, our_range_end};
    }

    auto get_args(int argc, char** argv) -> std::vector<std::string> {
        auto args = std::vector<char*>(static_cast<size_t>(argc));
        std::memcpy(args.data(),
                    argv,
                    static_cast<size_t>(argc) * sizeof(argv));
        auto ret = std::vector<std::string>();
        ret.reserve(static_cast<size_t>(argc));
        for(auto* arg : args) {
            auto str = std::string(arg);
            ret.emplace_back(std::move(str));
        }
        return ret;
    }

    parser::parser(const std::string& filename) {
        std::ifstream file(filename);
        assert(file.good());

        init(file);
    }

    parser::parser(std::istream& stream) {
        init(stream);
    }

    void parser::init(std::istream& stream) {
        std::string line;
        while(std::getline(stream, line)) {
            std::istringstream line_stream(line);
            std::string key;
            if(std::getline(line_stream, key, '=')) {
                std::string value;
                if(std::getline(line_stream, value)) {
                    m_options.emplace(key, parse_value(value));
                }
            }
        }
    }

    auto parser::get_string(const std::string& key) const
        -> std::optional<std::string> {
        return get_val<std::string>(key);
    }

    auto parser::get_ulong(const std::string& key) const
        -> std::optional<size_t> {
        return get_val<size_t>(key);
    }

    auto parser::get_endpoint(const std::string& key) const
        -> std::optional<network::endpoint_t> {
        const auto val_str = get_string(key);
        if(!val_str.has_value()) {
            return std::nullopt;
        }

        return parse_ip_port(val_str.value());
    }

    auto parser::get_loglevel(const std::string& key) const
        -> std::optional<logging::log_level> {
        const auto val_str = get_string(key);
        if(!val_str.has_value()) {
            return std::nullopt;
        }
        return logging::parse_loglevel(val_str.value());
    }

    auto parser::get_decimal(const std::string& key) const
        -> std::optional<double> {
        return get_val<double>(key);
    }

    auto parser::find_or_env(const std::string& key) const
        -> std::optional<value_t> {
        auto upper_key = key;
        std::transform(upper_key.begin(),
                       upper_key.end(),
                       upper_key.begin(),
                       [](unsigned char c) {
                           return std::toupper(c);
                       });
        if(const auto* env_v = std::getenv(upper_key.c_str())) {
            auto value = std::string(env_v);
            return parse_value(value);
        }

        auto it = m_options.find(key);
        if(it != m_options.end()) {
            return it->second;
        }

        return std::nullopt;
    }

    auto parser::parse_value(const std::string& value) -> value_t {
        if(value[0] != '\"' && value[value.size() - 1] != '\"') {
            if(value.find('.') == std::string::npos) {
                const auto as_int = static_cast<size_t>(std::stoull(value));
                return as_int;
            }
            const auto as_dbl = std::stod(value);
            return as_dbl;
        }

        const auto unquoted = value.substr(1, value.size() - 2);
        return unquoted;
    }
}
