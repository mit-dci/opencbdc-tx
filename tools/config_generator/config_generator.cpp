// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
//                    MITRE Corporation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config_generator.hpp"

#include "util/common/config.hpp"
#include "util/common/keys.hpp"
#include "util/common/variant_overloaded.hpp"
#include "util/network/tcp_listener.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <random>
#include <set>

// NORMAL CONFIGS

using namespace cbdc::config;

static constexpr auto MAX_SHARD_NUM = 256;
// Using the following values from src/common/config.hpp:

// "cbdc::config::two_phase_mode" : Name of value in config template file that
// is an integer which determines if this is 2PC or Atomizer. 1 means 2PC
// "cbdc::config::shard_count_key" : Number of shards to be created
// "cbdc::config::sentinel_count_key" : Number of sentinels to be created
// "cbdc::config::coordinator_count_key" : Number of coordinators to be created
// "cbdc::config::archiver_count_key" : Number of archivers to be created
// "cbdc::config::atomizer_count_key" : Number of atomizers to be created
// "cbdc::config::watchtower_count_key" : Number of watchtowers to be created

// Prefix of interest that denotes parameters in the file that are used
// here to help generate the config file but will not be present in the
// final product
static constexpr auto template_prefix = "tmpl_";

// TEMPLATE CONFIGS

// Parameter to tell us whether or not to randomize private/public key
// pairs, shard start - end and others
static constexpr auto tmpl_randomize_values = "tmpl_randomize_values";
// Average difference between shard_start and shard_end
static constexpr auto tmpl_shard_size = "tmpl_shard_size";
// Override for all log levels if they are not specified
static constexpr auto tmpl_universal_override_log_level
    = "tmpl_universal_override_log_level";
// Max number of raft replicated shards to make.
static constexpr auto tmpl_avg_shard_start_end_overlap_percent
    = "tmpl_avg_shard_start_end_overlap_percent";
// How march each shards coverage zones, roughly, are allowed to overlap.
static constexpr auto tmpl_max_shard_raft_replication_count
    = "tmpl_max_shard_raft_replication_count";
// Max number of raft replicated coordinators to make.
static constexpr auto tmpl_max_coordinator_raft_replication_count
    = "tmpl_max_coordinator_raft_replication_count";
// Default for all log levels if they are not specified
static constexpr auto tmpl_default_log_level = "tmpl_default_log_level";
static constexpr auto tmpl_sentinel_log_level = "tmpl_sentinel_log_level";
static constexpr auto tmpl_coordinator_log_level
    = "tmpl_coordinator_log_level";
static constexpr auto tmpl_shard_log_level = "tmpl_shard_log_level";
static constexpr auto tmpl_archiver_log_level = "tmpl_archiver_log_level";
static constexpr auto tmpl_atomizer_log_level = "tmpl_atomizer_log_level";
static constexpr auto tmpl_watchtower_log_level = "tmpl_watchtower_log_level";

// All acceptable log levels
static const std::set<std::string> log_levels
    = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

namespace cbdc::generate_config {

    config_generator::config_generator(std::string& _template_config_file,
                                       const size_t _start_port,
                                       std::string _build_dir)
        : m_template_config_file(_template_config_file),
          m_current_port(_start_port) {
        // Get Project root dir and build dir
        std::filesystem::path build_dir = std::filesystem::current_path();
        if(_build_dir.at(_build_dir.size() - 1) == '/') {
            _build_dir.erase(_build_dir.end() - 1);
        }
        // This config generator class assumes project root is "opencbdc-tx"
        while(build_dir.has_parent_path()) {
            if(build_dir.filename() != "opencbdc-tx") {
                build_dir = build_dir.parent_path();
            } else {
                m_project_root_dir = build_dir;
                std::string delimiter = "/";
                std::string tmp_str = _build_dir;
                size_t pos = 0;
                std::string token;
                while((pos = tmp_str.find('/')) != std::string::npos) {
                    token = tmp_str.substr(0, pos);
                    build_dir = build_dir.append(token);
                    tmp_str.erase(0, pos + delimiter.length());
                }
                token = tmp_str.substr(0, pos);
                build_dir = build_dir.append(token);
                tmp_str.erase(0, pos + delimiter.length());
                m_build_dir = build_dir;
                std::cout << "Build directory determined to be "
                          << m_build_dir.string() << std::endl;
                std::cout << "Project Root directory determined to be "
                          << m_project_root_dir.string() << std::endl;
                break;
            }
        }
        template_file_is_valid = true;
        if(!std::filesystem::exists(m_template_config_file)) {
            template_file_is_valid = false;
            std::filesystem::path temp_config_tools_dir = m_project_root_dir;
            temp_config_tools_dir.append("config").append("tools");
            std::filesystem::path temp_build_config_tools_dir = m_build_dir;
            temp_build_config_tools_dir.append("config").append("tools");
            std::cout << "Warning: File provided, " << m_template_config_file
                      << ", does not exist. Attempting to copy it from its "
                         "original location, "
                      << temp_config_tools_dir.string() << " to "
                      << temp_build_config_tools_dir.string() << std::endl;
            copy_templates_to_build_dir();
            // Try to use newly copied template files
            std::string delimiter = "/";
            std::string tmp_str = m_template_config_file;
            size_t pos = 0;
            std::string template_filename;
            while((pos = tmp_str.find('/')) != std::string::npos) {
                template_filename = tmp_str.substr(0, pos);
                tmp_str.erase(0, pos + delimiter.length());
            }
            template_filename = tmp_str.substr(0, pos);
            std::filesystem::path full_template_path_and_filename
                = temp_build_config_tools_dir.append(template_filename);
            if(std::filesystem::exists(full_template_path_and_filename)) {
                m_template_config_file
                    = full_template_path_and_filename.string();
                template_file_is_valid = true;
                std::cout << "Successfully copied " << template_filename
                          << " from " << temp_config_tools_dir.string()
                          << " to " << temp_build_config_tools_dir.string()
                          << ". Using "
                          << full_template_path_and_filename.string()
                          << " as template file." << std::endl;
            }
        }
    }

    void config_generator::calculate_shard_coverage(const size_t num_shards,
                                                    const size_t shard_size) {
        std::vector<size_t> shard_index_sum_total(shard_size, 0);
        shard_index_sum_total.reserve(shard_size);
        // Setup shard metadata
        for(size_t i = 0; i < num_shards; i++) {
            size_t randNum = rand() % (shard_size - 0 + 1) + 0;
            shard_info.emplace_back(ShardInfo());
            shard_info.at(i).coverage = std::vector<size_t>(MAX_SHARD_NUM, 0);
            shard_info.at(i).coverage.at(randNum) = 1;
            shard_info.at(i).still_expanding = true;
            shard_info.at(i).allow_overlap = false;
            shard_info.at(i).current_coverage_expansion_limits
                = std::make_pair(randNum, randNum);
            shard_info.at(i).numbers_covered = 1;
            shard_info.at(i).shard_id = i;
            auto overlap_percentage
                = find_value<double>(tmpl_avg_shard_start_end_overlap_percent);
            shard_info.at(i).overlap_percentage_allowed
                = std::abs(calculate_normal_distribution_point(
                    0,
                    overlap_percentage.value()));
            shard_index_sum_total.at(randNum)++;
        }
        auto still_expanding = true;
        while(still_expanding) {
            still_expanding = false;
            // Loop over all shards
            auto shard_it = shard_info.begin();
            while(shard_it != shard_info.end()) {
                // If all ORd still_expanding equal false, then they are all
                // false
                still_expanding |= shard_it->still_expanding;
                if(shard_it->still_expanding) {
                    // Next index to the upside in shard ids
                    size_t next_index_upside = std::min(
                        (shard_it->current_coverage_expansion_limits.second
                         + 1),
                        shard_size - 1);
                    // Next index to the downside in shard ids
                    size_t next_index_downside = std::min(
                        (shard_it->current_coverage_expansion_limits.first
                         - 1),
                        static_cast<size_t>(0));
                    // If shard can expand in coverage upwards and upwards
                    // coverage is more lightly covered than downwards
                    if(shard_it->current_coverage_expansion_limits.second
                           < (shard_size - 1)
                       && shard_index_sum_total.at(next_index_upside)
                              <= shard_index_sum_total.at(
                                  next_index_downside)) {
                        // If all values have been visited (overlap allowed) or
                        // has not yet been visited
                        if(shard_index_sum_total.at(
                               shard_it->current_coverage_expansion_limits
                                   .second
                               + 1)
                               == 0
                           || shard_it->allow_overlap) {
                            // Update expansion limits to upside
                            shard_it->current_coverage_expansion_limits
                                .second++;
                            // Update sum array of all shards
                            shard_index_sum_total.at(
                                shard_it->current_coverage_expansion_limits
                                    .second)++;
                            shard_it->numbers_covered += 1;
                            // Update coverage array
                            shard_it->coverage.at(
                                shard_it->current_coverage_expansion_limits
                                    .second)
                                = 1;
                        }
                        // Else if shard can expand in coverage downwards and
                    } else if(shard_it->current_coverage_expansion_limits.first
                              > 0) {
                        // If all values have been visited (overlap allowed) or
                        // has not yet been visited
                        if(shard_index_sum_total.at(
                               shard_it->current_coverage_expansion_limits
                                   .first
                               - 1)
                               == 0
                           || shard_it->allow_overlap) {
                            {
                                // Update expansion limits to downside
                                shard_it->current_coverage_expansion_limits
                                    .first--;
                                // Update sum array of all shards
                                shard_index_sum_total.at(
                                    shard_it->current_coverage_expansion_limits
                                        .first)++;
                                shard_it->numbers_covered += 1;
                                // Update coverage array
                                shard_it->coverage.at(
                                    shard_it->current_coverage_expansion_limits
                                        .first)
                                    = 1;
                            }
                        }
                    }
                }
                shard_bookkeeping(shard_index_sum_total, shard_it->shard_id);
                shard_it++;
            }
        }
    }

    void
    config_generator::shard_bookkeeping(const std::vector<size_t>& array_total,
                                        const size_t shard_id) {
        // Get sum of all coverage vectors for this shard over its expansion so
        // far
        double total_sum = 0;
        for(size_t i
            = shard_info.at(shard_id).current_coverage_expansion_limits.first;
            i <= shard_info.at(shard_id)
                     .current_coverage_expansion_limits.second;
            i++) {
            total_sum += static_cast<double>(array_total.at(i));
        }
        auto percentage_overlapped_so_far
            = (total_sum
               / static_cast<double>(shard_info.at(shard_id).numbers_covered))
            - 1.0;
        if(shard_info.at(shard_id).overlap_percentage_allowed
               <= percentage_overlapped_so_far
           || ((shard_info.at(shard_id)
                    .current_coverage_expansion_limits.second
                - shard_info.at(shard_id)
                      .current_coverage_expansion_limits.first)
               == (array_total.size() - 1))) {
            // This shard has reached its allowed overlap limit
            shard_info.at(shard_id).still_expanding = false;
        }
        // Check if all indices have been visited at least once from 0 to
        // shard_size
        auto all_visited = true;
        for(auto b : array_total) {
            if(b == 0) {
                all_visited = false;
                break;
            }
        }
        // If all indices have been visited at least once from 0 to
        // shard_size, allow shards to start overlapping in coverage
        if(all_visited) {
            auto shard_it = shard_info.begin();
            while(shard_it != shard_info.end()) {
                shard_it->allow_overlap = true;
                shard_it++;
            }
        }
    }

    // Get random value based on mean and standard deviation
    [[nodiscard]] auto
    config_generator::calculate_normal_distribution_point(const size_t mean,
                                                          const double std_dev)
        -> double {
        std::normal_distribution<double> distribution(
            static_cast<double>(mean),
            std_dev);
        return distribution(generator);
    }

    // This is not a failsafe because we are simply generating a
    // configuration file here, however, it is still good to check that the
    // port is available
    [[nodiscard]] auto config_generator::get_open_port() -> unsigned short {
        unsigned short port = m_current_port % MAX_PORT_NUM;
        m_current_port++;
        auto ep = cbdc::network::endpoint_t{cbdc::network::localhost, port};
        auto listener = cbdc::network::tcp_listener();
        while(!listener.listen(ep.first, ep.second)) {
            port = m_current_port % MAX_PORT_NUM;
            ep = cbdc::network::endpoint_t{cbdc::network::localhost, port};
            m_current_port++;
        }
        return port;
    }

    [[nodiscard]] auto config_generator::create_repeatable_key_pair()
        -> std::pair<std::string, std::string> {
        cbdc::privkey_t seckey;
        for(auto&& b : seckey) {
            b = rand();
        }
        cbdc::pubkey_t ret = cbdc::pubkey_from_privkey(seckey, m_secp.get());
        auto seckey_str = cbdc::to_string(seckey);
        auto pubkey_str = cbdc::to_string(ret);
        return std::make_pair(seckey_str, pubkey_str);
    }

    [[nodiscard]] auto config_generator::create_random_key_pair()
        -> std::pair<std::string, std::string> {
        std::uniform_int_distribution<unsigned char> keygen;

        cbdc::privkey_t seckey;
        for(auto&& b : seckey) {
            b = keygen(*m_random_source);
        }
        cbdc::pubkey_t ret = cbdc::pubkey_from_privkey(seckey, m_secp.get());
        auto seckey_str = cbdc::to_string(seckey);
        auto pubkey_str = cbdc::to_string(ret);
        return std::make_pair(seckey_str, pubkey_str);
    }

    [[nodiscard]] auto config_generator::create_key_pair() const
        -> std::pair<std::string, std::string> {
        if(m_randomize) {
            return create_random_key_pair();
        }
        return create_repeatable_key_pair();
    }

    [[nodiscard]] auto config_generator::parse_value(const std::string& value,
                                                     const bool keep_quotes)
        -> value_t {
        if(value[0] != '\"' && value[value.size() - 1] != '\"') {
            if(value.find('.') == std::string::npos) {
                const auto as_int = static_cast<size_t>(std::stoull(value));
                return as_int;
            }
            const auto as_dbl = std::stod(value);
            return as_dbl;
        }

        if(!keep_quotes) {
            const auto unquoted = value.substr(1, value.size() - 2);
            return unquoted;
        }
        return value;
    }

    [[nodiscard]] auto config_generator::get_param_from_template_file(
        const std::string& option,
        std::map<std::string, std::string>& config_map)
        -> std::variant<std::string, size_t, double> {
        auto it = config_map.find(option);
        if(it != config_map.end()) {
            value_t parsed_val = parse_value(it->second, false);
            if(std::holds_alternative<size_t>(parsed_val)) {
                return std::get<size_t>(parsed_val);
            }
            if(std::holds_alternative<double>(parsed_val)) {
                return std::get<double>(parsed_val);
            }
            if(std::holds_alternative<std::string>(parsed_val)) {
                return std::get<std::string>(parsed_val);
            }
            __builtin_unreachable();
        }
        auto error_msg = "Warning: Could not find param, " + option + ".";
        std::cout << error_msg << std::endl;
        return error_msg;
    }

    void config_generator::set_param_to_config_file(const std::string& key,
                                                    const std::string& value) {
        m_new_config << key << "=" << '"' << value << '"' << '\n';
    }

    void config_generator::set_param_to_config_file(const std::string& key,
                                                    const double value) {
        m_new_config << key << "=" << value << '\n';
    }

    void config_generator::set_param_to_config_file(const std::string& key,
                                                    const size_t value) {
        m_new_config << key << "=" << value << '\n';
    }

    void config_generator::set_log_level(const std::string& key,
                                         std::string& log_level) {
        if(log_levels.find(log_level) == log_levels.end()) {
            log_level = "DEBUG";
            std::cout << "Warning: Log level not recognized. Setting to DEBUG"
                      << std::endl;
        } else if(!find_value<std::string>(key).has_value()) {
            log_level
                = find_value<std::string>(tmpl_universal_override_log_level)
                      .value();
        }
    }

    [[maybe_unused]] auto
    config_generator::create_component(const char* type,
                                       const size_t component_count,
                                       bool create_2pc) -> std::string {
        std::string return_msg;
        auto _default_log_level
            = find_value<std::string>(tmpl_default_log_level);
        if(component_count == 0) {
            return_msg += "Warning: 0 count for at least one component. "
                          "Fix configuration template and rerun.\n";
        } else if(create_2pc) {
            for(size_t i = 0; i < component_count; i++) {
                if(type == shard_count_key) {
                    create_2pc_shard(_default_log_level.value(), i);
                } else if(type == sentinel_count_key) {
                    create_2pc_sentinel(_default_log_level.value(), i);
                } else if(type == coordinator_count_key) {
                    create_2pc_coordinator(_default_log_level.value(), i);
                } else {
                    std::cout << "Warning: Unrecognized component type, "
                              << type
                              << ", in Two-Phase Commit configuration "
                                 "generation."
                              << std::endl;
                }
            }
        } else {
            for(size_t i = 0; i < component_count; i++) {
                if(type == shard_count_key) {
                    create_atomizer_shard(_default_log_level.value(), i);
                } else if(type == sentinel_count_key) {
                    create_atomizer_sentienl(_default_log_level.value(), i);
                } else if(type == archiver_count_key) {
                    create_atomizer_archiver(_default_log_level.value(), i);
                } else if(type == atomizer_count_key) {
                    create_atomizer_atomizer(_default_log_level.value(), i);
                } else if(type == watchtower_count_key) {
                    create_atomizer_watchtower(_default_log_level.value(), i);
                } else {
                    std::cout << "Warning: Unrecognized component type, "
                              << type
                              << ", in Atomizer configuration generation."
                              << std::endl;
                }
            }
        }
        return return_msg;
    }

    void
    config_generator::create_2pc_shard(const std::string& default_log_level,
                                       size_t current_component_num) {
        auto shard_name = "shard" + std::to_string(current_component_num);
        auto endpoint_key = shard_name + "_endpoint";
        auto endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(endpoint_key, endpoint_val);
        auto raft_endpoint_key = shard_name + "_raft_endpoint";
        auto raft_endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(raft_endpoint_key, raft_endpoint_val);
        auto read_only_endpoint_key = shard_name + "_readonly_endpoint";
        auto read_only_endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(read_only_endpoint_key,
                                 read_only_endpoint_val);
        auto db_key = shard_name + "_db";
        set_param_to_config_file(db_key, db_key);
        auto log_level_key = shard_name + "_loglevel";
        auto log_level_val = default_log_level;
        set_log_level(tmpl_shard_log_level, log_level_val);
        set_param_to_config_file(log_level_key, log_level_val);
        auto count_key = shard_name + "_count";
        auto max_shard_raft_count
            = find_value<size_t>(tmpl_max_shard_raft_replication_count);
        size_t shard_raft_rep_count
            = (rand() % (max_shard_raft_count.value()) + 1);
        set_param_to_config_file(count_key, shard_raft_rep_count);
        auto start_key = shard_name + "_start";
        auto end_key = shard_name + "_end";
        set_param_to_config_file(start_key,
                                 shard_info.at(current_component_num)
                                     .current_coverage_expansion_limits.first);
        set_param_to_config_file(
            end_key,
            shard_info.at(current_component_num)
                .current_coverage_expansion_limits.second);
    }

    void
    config_generator::create_2pc_sentinel(const std::string& default_log_level,
                                          size_t current_component_num) {
        auto sentinel_name
            = "sentinel" + std::to_string(current_component_num);
        auto endpoint_key = sentinel_name + "_endpoint";
        auto endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(endpoint_key, endpoint_val);
        auto log_level_key = sentinel_name + "_loglevel";
        auto log_level_val = default_log_level;
        set_log_level(tmpl_sentinel_log_level, log_level_val);
        set_param_to_config_file(log_level_key, log_level_val);
        std::pair<std::string, std::string> key_pair = create_key_pair();
        auto private_key_key = sentinel_name + "_private_key";
        auto private_key_val = key_pair.first;
        set_param_to_config_file(private_key_key, private_key_val);
        auto public_key_key = sentinel_name + "_endpoint";
        auto public_key_val = key_pair.second;
        set_param_to_config_file(public_key_key, public_key_val);
    }

    void config_generator::create_2pc_coordinator(
        const std::string& default_log_level,
        size_t current_component_num) {
        auto coordinator_name
            = "coordinator" + std::to_string(current_component_num);
        auto endpoint_key = coordinator_name + "_endpoint";
        auto endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(endpoint_key, endpoint_val);
        auto raft_endpoint_key = coordinator_name + "_raft_endpoint";
        auto raft_endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(raft_endpoint_key, raft_endpoint_val);
        auto log_level_key = coordinator_name + "_loglevel";
        auto log_level_val = default_log_level;
        set_log_level(tmpl_coordinator_log_level, log_level_val);
        set_param_to_config_file(log_level_key, log_level_val);
        auto count_key = coordinator_name + "_count";
        auto max_coordinator_raft_count
            = find_value<size_t>(tmpl_max_coordinator_raft_replication_count);
        size_t coordinator_raft_rep_count
            = (rand() % (max_coordinator_raft_count.value()) + 1);
        set_param_to_config_file(count_key, coordinator_raft_rep_count);
        auto threads_key = coordinator_name + "_max_threads";
        size_t threads_val = 1;
        set_param_to_config_file(threads_key, threads_val);
    }

    void config_generator::create_atomizer_shard(
        const std::string& default_log_level,
        size_t current_component_num) {
        auto shard_name = "shard" + std::to_string(current_component_num);
        auto endpoint_key = shard_name + "_endpoint";
        auto endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(endpoint_key, endpoint_val);
        auto db_key = shard_name + "_db";
        set_param_to_config_file(db_key, db_key);
        auto log_level_key = shard_name + "_loglevel";
        auto log_level_val = default_log_level;
        set_log_level(tmpl_shard_log_level, log_level_val);
        set_param_to_config_file(log_level_key, log_level_val);
        auto start_key = shard_name + "_start";
        auto end_key = shard_name + "_end";
        set_param_to_config_file(start_key,
                                 shard_info.at(current_component_num)
                                     .current_coverage_expansion_limits.first);
        set_param_to_config_file(
            end_key,
            shard_info.at(current_component_num)
                .current_coverage_expansion_limits.second);
    }

    void config_generator::create_atomizer_sentienl(
        const std::string& default_log_level,
        size_t current_component_num) {
        auto sentinel_name
            = "sentinel" + std::to_string(current_component_num);
        auto endpoint_key = sentinel_name + "_endpoint";
        auto endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(endpoint_key, endpoint_val);
        auto log_level_key = sentinel_name + "_loglevel";
        auto log_level_val = default_log_level;
        set_log_level(tmpl_sentinel_log_level, log_level_val);
        set_param_to_config_file(log_level_key, log_level_val);
        std::pair<std::string, std::string> key_pair = create_key_pair();
        auto private_key_key = sentinel_name + "_private_key";
        auto private_key_val = key_pair.first;
        set_param_to_config_file(private_key_key, private_key_val);
        auto public_key_key = sentinel_name + "_endpoint";
        auto public_key_val = key_pair.second;
        set_param_to_config_file(public_key_key, public_key_val);
    }

    void config_generator::create_atomizer_archiver(
        const std::string& default_log_level,
        size_t current_component_num) {
        auto archive_name = "archiver" + std::to_string(current_component_num);
        auto endpoint_key = archive_name + "_endpoint";
        auto endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(endpoint_key, endpoint_val);
        auto db_key = archive_name + "_db";
        set_param_to_config_file(db_key, db_key);
        auto log_level_key = archive_name + "_loglevel";
        auto log_level_val = default_log_level;
        set_log_level(tmpl_archiver_log_level, log_level_val);
        set_param_to_config_file(log_level_key, log_level_val);
    }

    void config_generator::create_atomizer_atomizer(
        const std::string& default_log_level,
        size_t current_component_num) {
        auto atomizer_name
            = "atomizer" + std::to_string(current_component_num);
        auto endpoint_key = atomizer_name + "_endpoint";
        auto endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(endpoint_key, endpoint_val);
        auto raft_endpoint_key = atomizer_name + "_raft_endpoint";
        auto raft_endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(raft_endpoint_key, raft_endpoint_val);
        auto log_level_key = atomizer_name + "_loglevel";
        auto log_level_val = default_log_level;
        set_log_level(tmpl_atomizer_log_level, log_level_val);
        set_param_to_config_file(log_level_key, log_level_val);
    }

    void config_generator::create_atomizer_watchtower(
        const std::string& default_log_level,
        size_t current_component_num) {
        auto watchtower_name
            = "watchtower" + std::to_string(current_component_num);
        auto client_endpoint_key = watchtower_name + "_client_endpoint";
        auto client_endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(client_endpoint_key, client_endpoint_val);
        auto internal_endpoint_key = watchtower_name + "_internal_endpoint";
        auto internal_endpoint_val
            = cbdc::network::localhost + ":" + std::to_string(get_open_port());
        set_param_to_config_file(internal_endpoint_key, internal_endpoint_val);
        auto log_level_key = watchtower_name + "_loglevel";
        auto log_level_val = default_log_level;
        set_log_level(tmpl_watchtower_log_level, log_level_val);
        set_param_to_config_file(log_level_key, log_level_val);
    }

    void config_generator::load_template(
        const std::string& filename,
        std::map<std::string, std::string>& config_map) {
        std::ifstream file(filename);
        assert(file.good());
        std::string line;
        while(std::getline(file, line)) {
            std::istringstream line_stream(line);
            std::string key;
            if(std::getline(line_stream, key, '=')) {
                std::string value;
                if(std::getline(line_stream, value)) {
                    config_map.emplace(key, value);
                }
            }
        }
    }

    void config_generator::write_generated_config_to_file(
        const std::string& _config_file) {
        std::ofstream outFile;
        outFile.open(_config_file);
        outFile << m_new_config.str();
        outFile.close();
    }

    [[nodiscard]] auto
    config_generator::copy_to_build_dir(const std::string& filename) -> bool {
        std::filesystem::path cwd = std::filesystem::current_path();
        cwd.append(filename);
        if(!std::filesystem::exists(filename)) {
            return false;
        }
        const auto copyOptions
            = std::filesystem::copy_options::overwrite_existing;
        // Copy and remove file if we are not in build currently
        if(std::filesystem::current_path() != m_build_dir) {
            std::filesystem::copy(cwd, m_build_dir, copyOptions);
            std::filesystem::remove(cwd);
        }
        return true;
    }

    void config_generator::copy_templates_to_build_dir() {
        std::filesystem::path config_dir = m_project_root_dir;
        config_dir.append("config").append("tools");
        std::filesystem::path build_config_dir = m_build_dir;
        build_config_dir.append("config").append("tools");
        for(auto const& dir_entry :
            std::filesystem::directory_iterator{config_dir}) {
            std::string filename = dir_entry.path().filename();
            const auto* match_str = ".tmpl";
            std::string tmp_str
                = filename.substr(filename.size() - strlen(match_str),
                                  strlen(match_str));
            if(tmp_str == match_str) {
                const auto copyOptions
                    = std::filesystem::copy_options::overwrite_existing;
                std::filesystem::copy(dir_entry,
                                      build_config_dir,
                                      copyOptions);
                std::cout << "Copying " << dir_entry.path().string() << " to "
                          << build_config_dir.string() << std::endl;
            }
        }
    }

    template<typename T>
    [[nodiscard]] auto config_generator::find_value(const std::string& key)
        -> std::optional<T> {
        if(template_options.find(key) != template_options.end()) {
            if(std::holds_alternative<T>(template_options.at(key))) {
                return std::get<T>(template_options.at(key));
            }
            std::cout << "Warning: Unknown type for " << key
                      << " template parameter." << std::endl;
            return std::nullopt;
        }
        std::cout << "Warning: Missing " << key << " template parameter."
                  << std::endl;
        return std::nullopt;
    }

    [[maybe_unused]] auto config_generator::generate_configuration_file()
        -> std::string {
        // Cumulative return message
        std::string return_msg;
        const auto* output_filename = "tmp.cfg";

        if(!template_file_is_valid) {
            std::filesystem::path temp_build_dir = m_build_dir;
            temp_build_dir.append("config").append("tools");
            return_msg += "File provided, " + m_template_config_file
                        + ", did not exist and could not be copied to "
                        + temp_build_dir.string()
                        + ". Aborting operation. Please rerun with proper "
                          "template location \n";
            return return_msg;
        }

        std::map<std::string, std::string> config_map;
        load_template(m_template_config_file, config_map);
        // Deal with all config values in file with "tmpl_"
        auto config_map_it = config_map.begin();
        while(config_map_it != config_map.end()) {
            if(config_map_it->first.find(template_prefix)
               != std::string::npos) {
                template_options.emplace(
                    config_map_it->first,
                    parse_value(config_map_it->second, false));

            } else {
                value_t parsed_val = parse_value(config_map_it->second, true);
                if(std::holds_alternative<size_t>(parsed_val)) {
                    set_param_to_config_file(config_map_it->first,
                                             std::get<size_t>(parsed_val));
                } else if(std::holds_alternative<double>(parsed_val)) {
                    set_param_to_config_file(config_map_it->first,
                                             std::get<double>(parsed_val));
                } else if(std::holds_alternative<std::string>(parsed_val)) {
                    set_param_to_config_file(
                        config_map_it->first,
                        std::get<std::string>(parsed_val));
                } else {
                    __builtin_unreachable();
                }
            }
            config_map_it++;
        }

        auto randomize_int = find_value<std::size_t>(tmpl_randomize_values);
        m_randomize = randomize_int == 1;
        if(m_randomize) {
            srand(std::chrono::system_clock::now().time_since_epoch().count());
            generator.seed(
                std::chrono::system_clock::now().time_since_epoch().count());
        } else {
            generator.seed(1);
            srand(1);
        }

        // Create Components
        const auto is_two_phase_mode
            = get_param_from_template_file(two_phase_mode, config_map);
        const auto shard_count
            = get_param_from_template_file(shard_count_key, config_map);
        const auto sentinel_count
            = get_param_from_template_file(sentinel_count_key, config_map);

        auto shard_size = find_value<std::size_t>(tmpl_shard_size);
        // Add one since this is 255 and we are 0 indexing but want
        // 0-255 inclusive
        calculate_shard_coverage(std::get<size_t>(shard_count),
                                 shard_size.value() + 1);

        if(std::holds_alternative<size_t>(is_two_phase_mode)
           && std::get<size_t>(is_two_phase_mode) == 1) {
            output_filename = "2pc_generated_config.cfg";
            const auto coordinator_count
                = get_param_from_template_file(coordinator_count_key,
                                               config_map);
            return_msg += create_component(sentinel_count_key,
                                           std::get<size_t>(sentinel_count),
                                           true);
            return_msg += create_component(shard_count_key,
                                           std::get<size_t>(shard_count),
                                           true);
            return_msg += create_component(coordinator_count_key,
                                           std::get<size_t>(coordinator_count),
                                           true);
        } else {
            output_filename = "atomizer_generated_config.cfg";
            const auto atomizer_count
                = get_param_from_template_file(atomizer_count_key, config_map);
            const auto archiver_count
                = get_param_from_template_file(archiver_count_key, config_map);
            const auto watchtower_count
                = get_param_from_template_file(watchtower_count_key,
                                               config_map);
            return_msg += create_component(shard_count_key,
                                           std::get<size_t>(shard_count),
                                           false);
            return_msg += create_component(sentinel_count_key,
                                           std::get<size_t>(sentinel_count),
                                           false);
            return_msg += create_component(archiver_count_key,
                                           std::get<size_t>(archiver_count),
                                           false);
            return_msg += create_component(atomizer_count_key,
                                           std::get<size_t>(atomizer_count),
                                           false);
            return_msg += create_component(watchtower_count_key,
                                           std::get<size_t>(watchtower_count),
                                           false);
        }
        write_generated_config_to_file(output_filename);
        if(copy_to_build_dir(output_filename)) {
            return_msg += "SUCCESS";
            return return_msg;
        }
        return_msg += " Error: Failed to generate config file.";
        return return_msg;
    }
}
