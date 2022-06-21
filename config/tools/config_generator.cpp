// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
//                    MITRE Corporation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config_generator.hpp"

#include "util/common/keys.hpp"
#include "util/network/tcp_listener.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <functional>
#include <random>
#include <set>

// NORMAL CONFIGS

// Integer that determines if this is 2PC or Atomizer. 1 means 2PC
static constexpr auto two_phase_mode = "2pc";
// Number of shards to be created
static constexpr auto shard_count_key = "shard_count";
// Number of sentinels to be created
static constexpr auto sentinel_count_key = "sentinel_count";
// Number of coordinators to be created
static constexpr auto coordinator_count_key = "coordinator_count";
// Number of archivers to be created
static constexpr auto archiver_count_key = "archiver_count";
// Number of atomizers to be created
static constexpr auto atomizer_count_key = "atomizer_count";
// Number of watchtowers to be created
static constexpr auto watchtower_count_key = "watchtower_count";
// Prefix of interest that denotes parameters in the file that are used
// here to help generate the config file but will not be present in the
// final product
static constexpr auto template_prefix = "tmpl_";

// TEMPLATE CONFIGS

// Parameter to tell us whether or not to randomize private/public key
// pairs, shard start - end and others
static constexpr auto tmpl_randomize_values = "tmpl_randomize_values";
// ID number where first shard_start begins
static constexpr auto tmpl_shard_start = "tmpl_shard_start";
// Average difference between shard_start and shard_end
static constexpr auto tmpl_shard_size = "tmpl_shard_size";
// Override for all log levels if they are not specified
static constexpr auto tmpl_universal_override_log_level
    = "tmpl_universal_override_log_level";
// How march each shards coverage zones, roughly, are allowed to overlap.
static constexpr auto tmpl_avg_shard_start_end_overlap_percent
    = "tmpl_avg_shard_start_end_overlap_percent";
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
std::set<std::string> log_levels
    = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

namespace cbdc::generate_config {

    config_generator::config_generator(std::string& _template_config_file,
                                       const size_t _start_port)
        : m_template_config_file(_template_config_file),
          m_current_port(_start_port) {
        template_file_is_valid = true;
        if(!std::filesystem::exists(m_template_config_file)) {
            template_file_is_valid = false;
            std::cout << "Warning: File provided, " << m_template_config_file
                      << ", does not exist. Attempting to copy it from its "
                         "original location to build/config/tools. Rerun with "
                         "valid template file."
                      << std::endl;
            copy_templates_to_build_dir();
        }
        generator.seed(100);
    }

    void config_generator::calculate_shard_coverage(const size_t num_shards,
                                                    const bool randomize,
                                                    const size_t shard_size) {
        if(randomize) {
            srand(std::chrono::system_clock::now().time_since_epoch().count());
        }
        std::vector<size_t> shard_index_sum_total(shard_size, 0);
        shard_index_sum_total.reserve(shard_size);
        // Setup shard metadata
        for(size_t i = 0; i < num_shards; i++) {
            size_t randNum = rand() % (shard_size - 0 + 1) + 0;
            shard_info.push_back(ShardInfo());
            shard_info.at(i).coverage = std::vector<size_t>(256, 0);
            shard_info.at(i).coverage.at(randNum) = 1;
            shard_info.at(i).still_expanding = true;
            shard_info.at(i).allow_overlap = false;
            shard_info.at(i).current_coverage_expansion_limits
                = std::make_pair(randNum, randNum);
            shard_info.at(i).numbers_covered = 1;
            shard_info.at(i).shard_id = i;
            double overlap_percentage;
            find_value(tmpl_avg_shard_start_end_overlap_percent,
                       overlap_percentage);
            shard_info.at(i).overlap_percentage_allowed = std::abs(
                calculate_normal_distribution_point(0,
                                                    overlap_percentage,
                                                    randomize));
            shard_index_sum_total.at(randNum)++;
        }
        bool still_expanding = true;
        while(still_expanding) {
            still_expanding = false;
            // Loop over all shards
            for(std::vector<ShardInfo>::iterator shard_it = shard_info.begin();
                shard_it != shard_info.end();
                shard_it++) {
                // If all ORd still_expanding equal false, then they are all
                // false
                still_expanding |= shard_it->still_expanding;
                // Skip shard if it is no longer expanding
                if(shard_it->still_expanding == false) {
                    continue;
                } else {
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
                           || shard_it->allow_overlap == true) {
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
                           || shard_it->allow_overlap == true) {
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
            total_sum += array_total.at(i);
        }
        double percentage_overlapped_so_far
            = (total_sum
               / static_cast<double>(shard_info.at(shard_id).numbers_covered))
            - 1;
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
        bool all_visited = true;
        for(size_t i = 0; i < array_total.size(); i++) {
            if(array_total.at(i) == 0) {
                all_visited = false;
                break;
            }
        }
        // If all indices have been visited at least once from 0 to
        // shard_size, allow shards to start overlapping in coverage
        if(all_visited == true) {
            for(std::vector<ShardInfo>::iterator shard_it = shard_info.begin();
                shard_it != shard_info.end();
                shard_it++) {
                shard_it->allow_overlap = true;
            }
        }
    }

    // Get random value based on mean and standard deviation
    [[nodiscard]] auto
    config_generator::calculate_normal_distribution_point(const size_t mean,
                                                          const double std_dev,
                                                          const bool randomize)
        -> double {
        if(randomize) {
            generator.seed(
                std::chrono::system_clock::now().time_since_epoch().count());
        }
        std::normal_distribution<double> distribution(mean, std_dev);
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
        std::string seckey_str = cbdc::to_string(seckey);
        std::string pubkey_str = cbdc::to_string(ret);
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
        std::string seckey_str = cbdc::to_string(seckey);
        std::string pubkey_str = cbdc::to_string(ret);
        return std::make_pair(seckey_str, pubkey_str);
    }

    [[nodiscard]] auto config_generator::create_key_pair(const bool randomize)
        -> std::pair<std::string, std::string> {
        if(randomize) {
            return create_random_key_pair();
        } else {
            return create_repeatable_key_pair();
        }
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

        if(keep_quotes == false) {
            const auto unquoted = value.substr(1, value.size() - 2);
            return unquoted;
        }
        return value;
    }

    [[nodiscard]] auto config_generator::get_param_from_template_file(
        const std::string option,
        std::map<std::string, std::string>& config_map)
        -> std::variant<size_t, double, std::string> {
        auto it = config_map.find(option);
        if(it != config_map.end()) {
            value_t parsed_val = parse_value(it->second, false);
            if(std::holds_alternative<size_t>(parsed_val)) {
                return std::get<size_t>(parsed_val);
            } else if(std::holds_alternative<double>(parsed_val)) {
                return std::get<double>(parsed_val);
            } else if(std::holds_alternative<std::string>(parsed_val)) {
                return std::get<std::string>(parsed_val);
            } else {
                std::string error_msg
                    = "Warning: Unrecognized type for param, " + option + ".";
                std::cout << error_msg << std::endl;
                return error_msg;
            }
        } else {
            std::string error_msg
                = "Warning: Could not find param, " + option + ".";
            std::cout << error_msg << std::endl;
            return error_msg;
        }
    }

    void config_generator::set_param_to_config_file(const std::string key,
                                                    const std::string value) {
        m_new_config << key << "=" << '"' << value << '"' << '\n';
    }

    void config_generator::set_param_to_config_file(const std::string key,
                                                    const size_t value) {
        m_new_config << key << "=" << value << '\n';
    }

    void config_generator::set_param_to_config_file(const std::string key,
                                                    const double value) {
        m_new_config << key << "=" << value << '\n';
    }

    void config_generator::set_log_level(const std::string key,
                                         std::string& log_level) {
        if(find_value(key, log_level) == false) {
            find_value(tmpl_universal_override_log_level, log_level);
        }
        if(log_levels.find(log_level) == log_levels.end()) {
            log_level = "DEBUG";
            std::cout << "Warning: Log level not recognized. Setting to DEBUG"
                      << std::endl;
        }
    }

    // Method to create all the Two-Phase Commit related components for
    // generated config file
    void config_generator::create_2pc_component(const char* type,
                                                const size_t number) {
        size_t randomize_int = 0;
        find_value(tmpl_randomize_values, randomize_int);
        bool randomize = randomize_int == 1;
        auto _default_debug_level = static_cast<std::string>("INFO");
        find_value(tmpl_default_log_level, _default_debug_level);
        if(type == shard_count_key) {
            // Create Shards
            size_t shard_size;
            find_value(tmpl_shard_size, shard_size);
            // Add one since this is 255 and we are 0 indexing but want 0-255
            // inclusive
            shard_size += 1;
            calculate_shard_coverage(number, randomize, shard_size);
            for(size_t i = 0; i < number; i++) {
                std::string shard_name = "shard" + std::to_string(i);
                std::string endpoint_key = shard_name + "_endpoint";
                std::string endpoint_val = cbdc::network::localhost + ":"
                                         + std::to_string(get_open_port());
                set_param_to_config_file(endpoint_key, endpoint_val);
                std::string raft_endpoint_key = shard_name + "_raft_endpoint";
                std::string raft_endpoint_val
                    = cbdc::network::localhost + ":"
                    + std::to_string(get_open_port());
                set_param_to_config_file(raft_endpoint_key, raft_endpoint_val);
                std::string read_only_endpoint_key
                    = shard_name + "_readonly_endpoint";
                std::string read_only_endpoint_val
                    = cbdc::network::localhost + ":"
                    + std::to_string(get_open_port());
                set_param_to_config_file(read_only_endpoint_key,
                                         read_only_endpoint_val);
                std::string db_key = shard_name + "_db";
                set_param_to_config_file(db_key, db_key);
                std::string log_level_key = shard_name + "_loglevel";
                std::string log_level_val = _default_debug_level;
                set_log_level(tmpl_shard_log_level, log_level_val);
                set_param_to_config_file(log_level_key, log_level_val);
                std::string count_key = shard_name + "_count";
                size_t count_val = number;
                set_param_to_config_file(count_key, count_val);
                std::string start_key = shard_name + "_start";
                std::string end_key = shard_name + "_end";
                set_param_to_config_file(
                    start_key,
                    shard_info.at(i).current_coverage_expansion_limits.first);
                set_param_to_config_file(
                    end_key,
                    shard_info.at(i).current_coverage_expansion_limits.second);
            }
        } else if(type == sentinel_count_key) {
            // Create Sentinels
            for(size_t i = 0; i < number; i++) {
                std::string sentinel_name = "sentinel" + std::to_string(i);
                std::string endpoint_key = sentinel_name + "_endpoint";
                std::string endpoint_val = cbdc::network::localhost + ":"
                                         + std::to_string(get_open_port());
                set_param_to_config_file(endpoint_key, endpoint_val);
                std::string log_level_key = sentinel_name + "_loglevel";
                std::string log_level_val = _default_debug_level;
                set_log_level(tmpl_sentinel_log_level, log_level_val);
                set_param_to_config_file(log_level_key, log_level_val);
                std::pair<std::string, std::string> key_pair
                    = create_key_pair(randomize);
                std::string private_key_key = sentinel_name + "_private_key";
                std::string private_key_val = key_pair.first;
                set_param_to_config_file(private_key_key, private_key_val);
                std::string public_key_key = sentinel_name + "_endpoint";
                std::string public_key_val = key_pair.second;
                set_param_to_config_file(public_key_key, public_key_val);
            }
        } else if(type == coordinator_count_key) {
            // Create Coordinators
            for(size_t i = 0; i < number; i++) {
                std::string coordinator_name
                    = "coordinator" + std::to_string(i);
                std::string endpoint_key = coordinator_name + "_endpoint";
                std::string endpoint_val = cbdc::network::localhost + ":"
                                         + std::to_string(get_open_port());
                set_param_to_config_file(endpoint_key, endpoint_val);
                std::string raft_endpoint_key
                    = coordinator_name + "_raft_endpoint";
                std::string raft_endpoint_val
                    = cbdc::network::localhost + ":"
                    + std::to_string(get_open_port());
                set_param_to_config_file(raft_endpoint_key, raft_endpoint_val);
                std::string log_level_key = coordinator_name + "_loglevel";
                std::string log_level_val = _default_debug_level;
                set_log_level(tmpl_coordinator_log_level, log_level_val);
                set_param_to_config_file(log_level_key, log_level_val);
                std::string count_key = coordinator_name + "_count";
                size_t count_val = number;
                set_param_to_config_file(count_key, count_val);
                std::string threads_key = coordinator_name + "_max_threads";
                size_t threads_val = 1;
                set_param_to_config_file(threads_key, threads_val);
            }
        } else {
            std::cout << "Warning: Unrecognized component type, " << type
                      << ", in Two-Phase Commit configuration generation."
                      << std::endl;
        }
    }

    // Method to create all the Atomizer related components for generated
    // config file
    void config_generator::create_atomizer_component(const char* type,
                                                     const size_t number) {
        size_t randomize_int = 0;
        find_value(tmpl_randomize_values, randomize_int);
        bool randomize = randomize_int == 1;
        auto _default_debug_level = static_cast<std::string>("INFO");
        find_value(tmpl_default_log_level, _default_debug_level);
        if(type == shard_count_key) {
            // Create Shards
            size_t shard_size;
            find_value(tmpl_shard_size, shard_size);
            // Add one since this is 255 and we are 0 indexing but want 0-255
            // inclusive
            shard_size += 1;
            calculate_shard_coverage(number, randomize, shard_size);
            for(size_t i = 0; i < number; i++) {
                std::string shard_name = "shard" + std::to_string(i);
                std::string endpoint_key = shard_name + "_endpoint";
                std::string endpoint_val = cbdc::network::localhost + ":"
                                         + std::to_string(get_open_port());
                set_param_to_config_file(endpoint_key, endpoint_val);
                std::string db_key = shard_name + "_db";
                set_param_to_config_file(db_key, db_key);
                std::string log_level_key = shard_name + "_loglevel";
                std::string log_level_val = _default_debug_level;
                set_log_level(tmpl_shard_log_level, log_level_val);
                set_param_to_config_file(log_level_key, log_level_val);
                std::string start_key = shard_name + "_start";
                std::string end_key = shard_name + "_end";
                set_param_to_config_file(
                    start_key,
                    shard_info.at(i).current_coverage_expansion_limits.first);
                set_param_to_config_file(
                    end_key,
                    shard_info.at(i).current_coverage_expansion_limits.second);
            }
        } else if(type == sentinel_count_key) {
            // Create Sentinels
            for(size_t i = 0; i < number; i++) {
                std::string sentinel_name = "sentinel" + std::to_string(i);
                std::string endpoint_key = sentinel_name + "_endpoint";
                std::string endpoint_val = cbdc::network::localhost + ":"
                                         + std::to_string(get_open_port());
                set_param_to_config_file(endpoint_key, endpoint_val);
                std::string log_level_key = sentinel_name + "_loglevel";
                std::string log_level_val = _default_debug_level;
                set_log_level(tmpl_sentinel_log_level, log_level_val);
                set_param_to_config_file(log_level_key, log_level_val);
                std::pair<std::string, std::string> key_pair
                    = create_key_pair(randomize);
                std::string private_key_key = sentinel_name + "_private_key";
                std::string private_key_val = key_pair.first;
                set_param_to_config_file(private_key_key, private_key_val);
                std::string public_key_key = sentinel_name + "_endpoint";
                std::string public_key_val = key_pair.second;
                set_param_to_config_file(public_key_key, public_key_val);
            }
        } else if(type == archiver_count_key) {
            // Create Archiver(s)
            for(size_t i = 0; i < number; i++) {
                std::string archive_name = "archiver" + std::to_string(i);
                std::string endpoint_key = archive_name + "_endpoint";
                std::string endpoint_val = cbdc::network::localhost + ":"
                                         + std::to_string(get_open_port());
                set_param_to_config_file(endpoint_key, endpoint_val);
                std::string db_key = archive_name + "_db";
                set_param_to_config_file(db_key, db_key);
                std::string log_level_key = archive_name + "_loglevel";
                std::string log_level_val = _default_debug_level;
                set_log_level(tmpl_archiver_log_level, log_level_val);
                set_param_to_config_file(log_level_key, log_level_val);
            }
        } else if(type == atomizer_count_key) {
            // Create Atomizers
            for(size_t i = 0; i < number; i++) {
                std::string atomizer_name = "atomizer" + std::to_string(i);
                std::string endpoint_key = atomizer_name + "_endpoint";
                std::string endpoint_val = cbdc::network::localhost + ":"
                                         + std::to_string(get_open_port());
                set_param_to_config_file(endpoint_key, endpoint_val);
                std::string raft_endpoint_key
                    = atomizer_name + "_raft_endpoint";
                std::string raft_endpoint_val
                    = cbdc::network::localhost + ":"
                    + std::to_string(get_open_port());
                set_param_to_config_file(raft_endpoint_key, raft_endpoint_val);
                std::string log_level_key = atomizer_name + "_loglevel";
                std::string log_level_val = _default_debug_level;
                set_log_level(tmpl_atomizer_log_level, log_level_val);
                set_param_to_config_file(log_level_key, log_level_val);
            }
        } else if(type == watchtower_count_key) {
            // Create Watchtowers
            for(size_t i = 0; i < number; i++) {
                std::string watchtower_name = "watchtower" + std::to_string(i);
                std::string client_endpoint_key
                    = watchtower_name + "_client_endpoint";
                std::string client_endpoint_val
                    = cbdc::network::localhost + ":"
                    + std::to_string(get_open_port());
                set_param_to_config_file(client_endpoint_key,
                                         client_endpoint_val);
                std::string internal_endpoint_key
                    = watchtower_name + "_internal_endpoint";
                std::string internal_endpoint_val
                    = cbdc::network::localhost + ":"
                    + std::to_string(get_open_port());
                set_param_to_config_file(internal_endpoint_key,
                                         internal_endpoint_val);
                std::string log_level_key = watchtower_name + "_loglevel";
                std::string log_level_val = _default_debug_level;
                set_log_level(tmpl_watchtower_log_level, log_level_val);
                set_param_to_config_file(log_level_key, log_level_val);
            }
        } else {
            std::cout << "Warning: Unrecognized component type, " << type
                      << ", in Atomizer configuration generation."
                      << std::endl;
        }
    }

    void config_generator::load_template(
        const std::string filename,
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

    // This method assumes project root is "opencbdc-tx" and build dir is
    // "build"
    [[nodiscard]] auto
    config_generator::copy_to_build_dir(const std::string filename) -> bool {
        std::filesystem::path cwd = std::filesystem::current_path();
        cwd.append(filename);
        std::filesystem::path build_dir = std::filesystem::current_path();
        while(build_dir.has_parent_path()) {
            if(build_dir.filename() == "opencbdc-tx") {
                build_dir = build_dir.append("build");
                break;
            } else {
                build_dir = build_dir.parent_path();
            }
        }
        if(std::filesystem::exists(filename)) {
            const auto copyOptions
                = std::filesystem::copy_options::overwrite_existing;
            // Copy and remove file if we are not in build currently
            if(std::filesystem::current_path().filename() != "build") {
                std::filesystem::copy(cwd, build_dir, copyOptions);
                std::filesystem::remove(cwd);
            }
            return true;
        } else {
            return false;
        }
    }

    // This method assumes project root is "opencbdc-tx" and build dir is
    // "build"
    void config_generator::copy_templates_to_build_dir() {
        std::filesystem::path config_dir = std::filesystem::current_path();
        std::filesystem::path build_dir = std::filesystem::current_path();
        while(config_dir.has_parent_path()) {
            if(config_dir.filename() == "opencbdc-tx") {
                config_dir = config_dir.append("config");
                config_dir = config_dir.append("tools");
                build_dir = build_dir.append("build");
                build_dir = build_dir.append("config");
                build_dir = build_dir.append("tools");
                break;
            } else {
                config_dir = config_dir.parent_path();
                build_dir = build_dir.parent_path();
            }
        }
        for(auto const& dir_entry :
            std::filesystem::directory_iterator{config_dir}) {
            std::string filename = dir_entry.path().filename();
            std::string match_str = ".tmpl";
            std::string tmp_str = filename.substr(filename.size() - 5, 5);
            if(tmp_str == match_str) {
                const auto copyOptions
                    = std::filesystem::copy_options::overwrite_existing;
                std::filesystem::copy(dir_entry, build_dir, copyOptions);
            }
        }
    }

    template<typename T>
    [[maybe_unused]] auto config_generator::find_value(const std::string key,
                                                       T& output) -> bool {
        if(template_options.find(key) != template_options.end()) {
            if(std::holds_alternative<T>(template_options.at(key))) {
                output = std::get<T>(template_options.at(key));
                return true;
            } else {
                std::cout << "Warning: Unknown type for " << key
                          << " template parameter." << std::endl;
                return false;
            }
        } else {
            std::cout << "Warning: Missing " << key << " template parameter."
                      << std::endl;
            return false;
        }
    }

    [[maybe_unused]] auto config_generator::generate_configuration_file()
        -> std::string {
        // Cumulative return message
        std::string return_msg = "";
        std::string output_filename = "tmp.cfg";

        if(!template_file_is_valid) {
            return_msg += "File provided, " + m_template_config_file
                        + ", does not exist. Aborting operation. \n";
            return return_msg;
        }
        std::map<std::string, std::string> config_map;
        load_template(m_template_config_file, config_map);
        // Deal with all config values in file with "tmpl_"
        for(auto config_map_it = config_map.begin();
            config_map_it != config_map.end();
            config_map_it++) {
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
                    return_msg += "Warning: Unrecognized type for param, "
                                + config_map_it->first + ". \n";
                }
            }
        }

        // Create Components
        const auto is_two_phase_mode
            = get_param_from_template_file(two_phase_mode, config_map);
        const auto shard_count
            = get_param_from_template_file(shard_count_key, config_map);
        const auto sentinel_count
            = get_param_from_template_file(sentinel_count_key, config_map);

        if(std::holds_alternative<size_t>(is_two_phase_mode)
           && std::get<size_t>(is_two_phase_mode) == 1) {
            output_filename = "2pc_generated_config.cfg";
            const auto coordinator_count
                = get_param_from_template_file(coordinator_count_key,
                                               config_map);
            if(std::holds_alternative<size_t>(sentinel_count)) {
                if(std::get<size_t>(sentinel_count) == 0) {
                    return_msg
                        += "Warning: Two-phase mode requires at least one "
                           "configured sentinel. Fix configuration "
                           "template and rerun.\n";
                    return return_msg;
                } else {
                    create_2pc_component(sentinel_count_key,
                                         std::get<size_t>(sentinel_count));
                }
            }
            if(std::holds_alternative<size_t>(shard_count)) {
                if(std::get<size_t>(shard_count) == 0) {
                    return_msg
                        += "Warning: Two-phase mode requires at least one "
                           "configured shard. Fix configuration template "
                           "and "
                           "rerun.\n";
                    return return_msg;
                } else {
                    create_2pc_component(shard_count_key,
                                         std::get<size_t>(shard_count));
                }
            }
            if(std::holds_alternative<size_t>(coordinator_count)) {
                if(std::get<size_t>(coordinator_count) == 0) {
                    return_msg
                        += "Warning: Two-phase mode required at least one "
                           "configured coordinator. Fix configuration "
                           "template and rerun.\n";
                    return return_msg;
                } else {
                    create_2pc_component(coordinator_count_key,
                                         std::get<size_t>(coordinator_count));
                }
            }
        } else {
            output_filename = "atomizer_generated_config.cfg";
            const auto atomizer_count
                = get_param_from_template_file(atomizer_count_key, config_map);
            const auto archiver_count
                = get_param_from_template_file(archiver_count_key, config_map);
            const auto watchtower_count
                = get_param_from_template_file(watchtower_count_key,
                                               config_map);

            if(std::holds_alternative<size_t>(watchtower_count)) {
                if(std::get<size_t>(watchtower_count) == 0) {
                    return_msg += "Warning: Atomizer mode requires at least "
                                  "one configured watchtower. Fix "
                                  "configuration template and rerun. \n";
                    return return_msg;
                } else {
                    create_atomizer_component(
                        watchtower_count_key,
                        std::get<size_t>(watchtower_count));
                }
            }
            if(std::holds_alternative<size_t>(archiver_count)) {
                if(std::get<size_t>(archiver_count) == 0) {
                    return_msg += "Warning: Atomizer mode requires at least "
                                  "one configured archiver. Fix configuration "
                                  "template and rerun. \n";
                    return return_msg;
                } else {
                    create_atomizer_component(
                        archiver_count_key,
                        std::get<size_t>(archiver_count));
                }
            }
            if(std::holds_alternative<size_t>(sentinel_count)) {
                if(std::get<size_t>(sentinel_count) == 0) {
                    return_msg
                        += "Warning: Sentinels require at least one "
                           "configured shard. Fix configuration template "
                           "and "
                           "rerun. \n";
                    return return_msg;
                } else {
                    create_atomizer_component(
                        sentinel_count_key,
                        std::get<size_t>(sentinel_count));
                }
            }
            if(std::holds_alternative<size_t>(atomizer_count)) {
                if(std::get<size_t>(atomizer_count) == 0) {
                    return_msg += "Warning: Atomizer mode requires at least "
                                  "one configured atomizer. Fix configuration "
                                  "template and rerun. \n";
                    return return_msg;
                } else {
                    create_atomizer_component(
                        atomizer_count_key,
                        std::get<size_t>(atomizer_count));
                }
            }
            if(std::holds_alternative<size_t>(shard_count)) {
                create_atomizer_component(shard_count_key,
                                          std::get<size_t>(shard_count));
            }
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

[[maybe_unused]] auto find_value(const std::string key, std::string& output)
    -> bool;
[[maybe_unused]] auto find_value(const std::string key, double& output)
    -> bool;
[[maybe_unused]] auto find_value(const std::string key, size_t& output)
    -> bool;
