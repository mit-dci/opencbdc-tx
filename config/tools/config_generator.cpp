#include "util/common/config.hpp"
#include "util/common/keys.hpp"
#include "util/common/random_source.hpp"
#include "util/network/tcp_listener.hpp"

#include <cassert>
#include <random>
#include <secp256k1.h>
#include <set>

// TODO: Cleanup (try to make simpler). Format properly.

// TODO what is loadgen_sendtx_output_count, loadgen_sendtx_input_count,
// loadgen_invalid_tx_rate and loadgen_fixed_tx_rate?

#define MAX_PORT 65535
unsigned short current_port = 5555;
std::mt19937 rng;
static const inline auto m_random_source
    = std::make_unique<cbdc::random_source>(cbdc::config::random_source);
static const inline auto m_secp
    = std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)>(
        secp256k1_context_create(SECP256K1_CONTEXT_SIGN),
        &secp256k1_context_destroy);

std::stringstream new_config;

std::set<std::string> integer_configs = {"archiver_count",
                                         "atomizer_count",
                                         "shard_count",
                                         "sentinel_count",
                                         "watchtower_count",
                                         "window_size",
                                         "snapshot_distance",
                                         "raft_max_batch",
                                         "heartbeat",
                                         "election_timeout_lower",
                                         "election_timeout_upper",
                                         "target_block_interval",
                                         "stxo_cache_depth",
                                         "initial_mint_count",
                                         "initial_mint_value",
                                         "loadgen_sendtx_output_count",
                                         "loadgen_sendtx_input_count",
                                         "tmpl_randomize_values",
                                         "tmpl_shard_start",
                                         "tmpl_shard_size"};

std::set<std::string> double_configs
    = {"loadgen_invalid_tx_rate",
       "loadgen_fixed_tx_rate",
       "tmpl_avg_shard_start_end_overlap_percent"};

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
// Prefix of interest that denotes parameters in the file that are used here to
// help generate the config file but will not be present in the final product
static constexpr auto template_prefix = "tmpl_";

typedef struct regular_options_of_interest {
    static constexpr auto m_two_phase_mode{two_phase_mode};
    static constexpr auto m_shard_count_key{shard_count_key};
    static constexpr auto m_sentinel_count_key{sentinel_count_key};
    static constexpr auto m_coordinator_count_key{coordinator_count_key};
    static constexpr auto m_archiver_count_key{archiver_count_key};
    static constexpr auto m_atomizer_count_key{atomizer_count_key};
    static constexpr auto m_watchtower_count_key{watchtower_count_key};
    static constexpr auto m_template_prefix{template_prefix};
} RegularOptions;

// Parameter to tell us whether or not to randomize private/public key pairs,
// shard start - end and others
static constexpr auto tmpl_randomize_values = "tmpl_randomize_values";
// ID number where first shard_start begins
static constexpr auto tmpl_shard_start = "tmpl_shard_start";
// Average difference between shard_start and shard_end
static constexpr auto tmpl_shard_size = "tmpl_shard_size";
// Average overlap between last shard_start - shard_end and next shard_start
// and shard_end
// TODO what is importance of this overlap? I have noticed that making the
// overlap too large ruins unit tests.
static constexpr auto tmpl_avg_shard_start_end_overlap_percent
    = "tmpl_avg_shard_start_end_overlap_percent";

using value_t = std::variant<std::string, size_t, double>;

std::map<std::string, value_t> template_options;

auto calculate_normal_distribution_point(size_t mean,
                                         double std_dev,
                                         bool randomize) -> size_t {
    std::default_random_engine generator;
    if(randomize) {
        generator.seed(
            std::chrono::system_clock::now().time_since_epoch().count());
    }
    std::normal_distribution<double> distribution(mean, std_dev);
    size_t ret_val = 0;
    ret_val = std::round(distribution(generator));
    return ret_val;
}

auto get_open_port() -> unsigned short {
    unsigned short port = current_port % MAX_PORT;
    current_port++;
    auto ep = cbdc::network::endpoint_t{cbdc::network::localhost, port};
    auto listener = cbdc::network::tcp_listener();
    while(!listener.listen(ep.first, ep.second)) {
        port = current_port % MAX_PORT;
        ep = cbdc::network::endpoint_t{cbdc::network::localhost, port};
        current_port++;
    }
    return port;
}

[[nodiscard]] auto create_repeatable_key_pair()
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

[[nodiscard]] auto create_random_key_pair()
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

[[nodiscard]] auto create_key_pair(bool randomize)
    -> std::pair<std::string, std::string> {
    if(randomize) {
        return create_random_key_pair();
    } else {
        return create_repeatable_key_pair();
    }
}

[[nodiscard]] auto get_int_param_from_template_file(
    const std::string option,
    std::map<std::string, std::string>& config_map)
    -> std::variant<size_t, std::string> {
    auto it = config_map.find(option);
    if(it != config_map.end()) {
        return static_cast<size_t>(std::stoi(it->second));
    } else {
        return "Error finding " + option;
    }
}

[[nodiscard]] auto get_double_param_from_template_file(
    const std::string option,
    std::map<std::string, std::string>& config_map)
    -> std::variant<double, std::string> {
    auto it = config_map.find(option);
    if(it != config_map.end()) {
        return std::stod(it->second);
    } else {
        return "Error finding " + option;
    }
}

void set_param_to_config_file(std::string key, std::string value) {
    new_config << key << "=" << '"' << value << '"' << '\n';
}

void set_param_to_config_file(std::string key, size_t value) {
    new_config << key << "=" << value << '\n';
}

void set_param_to_config_file(std::string key, double value) {
    new_config << key << "=" << value << '\n';
}

void create_2pc_component(const char* type, int number) {
    if(type == shard_count_key) {
        size_t start_val = std::get<size_t>(
            template_options.find(tmpl_shard_start)->second);
        size_t end_val
            = std::get<size_t>(template_options.find(tmpl_shard_size)->second);
        size_t avg_size_shard_start_to_end = end_val;
        double overlap_percentage = std::get<double>(
            template_options.find(tmpl_avg_shard_start_end_overlap_percent)
                ->second);
        for(int i = 0; i < number; i++) {
            std::string shard_name = "shard" + std::to_string(i);
            std::string endpoint_key = shard_name + "_endpoint";
            std::string endpoint_val = cbdc::network::localhost + ":"
                                     + std::to_string(get_open_port());
            set_param_to_config_file(endpoint_key, endpoint_val);
            std::string raft_endpoint_key = shard_name + "_raft_endpoint";
            std::string raft_endpoint_val = cbdc::network::localhost + ":"
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
            std::string log_level_val = "DEBUG";
            set_param_to_config_file(log_level_key, log_level_val);
            std::string start_key = shard_name + "_start";
            set_param_to_config_file(start_key, start_val);
            end_val = start_val + avg_size_shard_start_to_end;
            std::string end_key = shard_name + "_end";
            set_param_to_config_file(end_key, end_val);
            start_val = calculate_normal_distribution_point(
                end_val * (1 - overlap_percentage / 10),
                avg_size_shard_start_to_end * overlap_percentage,
                std::get<size_t>(template_options.at(tmpl_randomize_values))
                    == 1);
            std::string count_key = shard_name + "_count";
            size_t count_val = number;
            set_param_to_config_file(count_key, count_val);
        }
    } else if(type == sentinel_count_key) {
        for(int i = 0; i < number; i++) {
            std::string sentinel_name = "sentinel" + std::to_string(i);
            std::string endpoint_key = sentinel_name + "_endpoint";
            std::string endpoint_val = cbdc::network::localhost + ":"
                                     + std::to_string(get_open_port());
            set_param_to_config_file(endpoint_key, endpoint_val);
            std::string log_level_key = sentinel_name + "_loglevel";
            std::string log_level_val = "WARN";
            set_param_to_config_file(log_level_key, log_level_val);
            ;
            std::pair<std::string, std::string> key_pair = create_key_pair(
                std::get<size_t>(template_options.at(tmpl_randomize_values))
                == 1);
            std::string private_key_key = sentinel_name + "_private_key";
            std::string private_key_val = key_pair.first;
            set_param_to_config_file(private_key_key, private_key_val);
            std::string public_key_key = sentinel_name + "_endpoint";
            std::string public_key_val = key_pair.second;
            set_param_to_config_file(public_key_key, public_key_val);
        }
    } else if(type == coordinator_count_key) {
        for(int i = 0; i < number; i++) {
            std::string coordinator_name = "coordinator" + std::to_string(i);
            std::string endpoint_key = coordinator_name + "_endpoint";
            std::string endpoint_val = cbdc::network::localhost + ":"
                                     + std::to_string(get_open_port());
            set_param_to_config_file(endpoint_key, endpoint_val);
            std::string raft_endpoint_key
                = coordinator_name + "_raft_endpoint";
            std::string raft_endpoint_val = cbdc::network::localhost + ":"
                                          + std::to_string(get_open_port());
            set_param_to_config_file(raft_endpoint_key, raft_endpoint_val);
            std::string log_level_key = coordinator_name + "_loglevel";
            std::string log_level_val = "DEBUG";
            set_param_to_config_file(log_level_key, log_level_val);
            // TODO What is this extra count?
            std::string count_key = coordinator_name + "_count";
            size_t count_val = number;
            set_param_to_config_file(count_key, count_val);
            std::string threads_key = coordinator_name + "_max_threads";
            size_t threads_val = 1;
            set_param_to_config_file(threads_key, threads_val);
        }
    } else {
        // TODO WARN
    }
}

void create_atomizer_component(const char* type, int number) {
    if(type == shard_count_key) {
        size_t start_val = std::get<size_t>(
            template_options.find(tmpl_shard_start)->second);
        size_t end_val
            = std::get<size_t>(template_options.find(tmpl_shard_size)->second);
        size_t avg_size_shard_start_to_end = end_val;
        double overlap_percentage = std::get<double>(
            template_options.find(tmpl_avg_shard_start_end_overlap_percent)
                ->second);
        for(int i = 0; i < number; i++) {
            std::string shard_name = "shard" + std::to_string(i);
            std::string endpoint_key = shard_name + "_endpoint";
            std::string endpoint_val = cbdc::network::localhost + ":"
                                     + std::to_string(get_open_port());
            set_param_to_config_file(endpoint_key, endpoint_val);
            std::string db_key = shard_name + "_db";
            set_param_to_config_file(db_key, db_key);
            std::string log_level_key = shard_name + "_loglevel";
            std::string log_level_val = "WARN";
            set_param_to_config_file(log_level_key, log_level_val);
            std::string start_key = shard_name + "_start";
            set_param_to_config_file(start_key, start_val);
            end_val = start_val + avg_size_shard_start_to_end;
            std::string end_key = shard_name + "_end";
            set_param_to_config_file(end_key, end_val);
            start_val = calculate_normal_distribution_point(
                end_val * (1 - overlap_percentage / 10),
                avg_size_shard_start_to_end * overlap_percentage,
                std::get<size_t>(template_options.at(tmpl_randomize_values))
                    == 1);
        }
    } else if(type == sentinel_count_key) {
        for(int i = 0; i < number; i++) {
            std::string sentinel_name = "sentinel" + std::to_string(i);
            std::string endpoint_key = sentinel_name + "_endpoint";
            std::string endpoint_val = cbdc::network::localhost + ":"
                                     + std::to_string(get_open_port());
            set_param_to_config_file(endpoint_key, endpoint_val);
            std::string log_level_key = sentinel_name + "_loglevel";
            std::string log_level_val = "WARN";
            set_param_to_config_file(log_level_key, log_level_val);
            std::pair<std::string, std::string> key_pair = create_key_pair(
                std::get<size_t>(template_options.at(tmpl_randomize_values))
                == 1);
            std::string private_key_key = sentinel_name + "_private_key";
            std::string private_key_val = key_pair.first;
            set_param_to_config_file(private_key_key, private_key_val);
            std::string public_key_key = sentinel_name + "_endpoint";
            std::string public_key_val = key_pair.second;
            set_param_to_config_file(public_key_key, public_key_val);
        }
    } else if(type == archiver_count_key) {
        for(int i = 0; i < number; i++) {
            std::string archive_name = "archiver" + std::to_string(i);
            std::string endpoint_key = archive_name + "_endpoint";
            std::string endpoint_val = cbdc::network::localhost + ":"
                                     + std::to_string(get_open_port());
            set_param_to_config_file(endpoint_key, endpoint_val);
            std::string db_key = archive_name + "_db";
            set_param_to_config_file(db_key, db_key);
        }
    } else if(type == atomizer_count_key) {
        for(int i = 0; i < number; i++) {
            std::string atomizer_name = "atomizer" + std::to_string(i);
            std::string endpoint_key = atomizer_name + "_endpoint";
            std::string endpoint_val = cbdc::network::localhost + ":"
                                     + std::to_string(get_open_port());
            set_param_to_config_file(endpoint_key, endpoint_val);
            std::string raft_endpoint_key = atomizer_name + "_raft_endpoint";
            std::string raft_endpoint_val = cbdc::network::localhost + ":"
                                          + std::to_string(get_open_port());
            set_param_to_config_file(raft_endpoint_key, raft_endpoint_val);
        }
    } else if(type == watchtower_count_key) {
        for(int i = 0; i < number; i++) {
            std::string watchtower_name = "watchtower" + std::to_string(i);
            std::string client_endpoint_key
                = watchtower_name + "_client_endpoint";
            std::string client_endpoint_val = cbdc::network::localhost + ":"
                                            + std::to_string(get_open_port());
            set_param_to_config_file(client_endpoint_key, client_endpoint_val);
            std::string internal_endpoint_key
                = watchtower_name + "_internal_endpoint";
            std::string internal_endpoint_val
                = cbdc::network::localhost + ":"
                + std::to_string(get_open_port());
            set_param_to_config_file(internal_endpoint_key,
                                     internal_endpoint_val);
            std::string log_level_key = watchtower_name + "_loglevel";
            std::string log_level_val = "TRACE";
            set_param_to_config_file(log_level_key, log_level_val);
        }
    } else {
        // TODO WARN
    }
}

void write_options_to_config_file(std::string& config_file) {
    std::ofstream outFile;
    outFile.open(config_file);
    outFile << new_config.str();
    outFile.close();
}

void load_config(std::string filename,
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

auto generate_configuration_file(const std::string& config_file)
    -> std::string {
    std::string output_filename;
    auto regular_opts = regular_options_of_interest{};
    std::map<std::string, std::string> config_map;
    load_config(config_file, config_map);

    // Deal with all config values in file with "tmpl_"
    for(auto config_map_it = config_map.begin();
        config_map_it != config_map.end();
        config_map_it++) {
        if(config_map_it->first.find(regular_opts.m_template_prefix)
           != std::string::npos) {
            if(integer_configs.find(config_map_it->first)
               != integer_configs.end()) {
                template_options.emplace(
                    config_map_it->first,
                    static_cast<size_t>(std::stoull(config_map_it->second)));
            } else if(double_configs.find(config_map_it->first)
                      != integer_configs.end()) {
                template_options.emplace(config_map_it->first,
                                         std::stod(config_map_it->second));
            } else {
                template_options.emplace(config_map_it->first,
                                         config_map_it->second);
            }
        } else {
            if(integer_configs.find(config_map_it->first)
               != integer_configs.end()) {
                set_param_to_config_file(
                    config_map_it->first,
                    static_cast<size_t>(std::stoull(config_map_it->second)));
            } else if(double_configs.find(config_map_it->first)
                      != integer_configs.end()) {
                set_param_to_config_file(config_map_it->first,
                                         std::stod(config_map_it->second));
            } else {
                set_param_to_config_file(config_map_it->first,
                                         config_map_it->second);
            }
        }
    }

    // Create Components
    const auto is_two_phase_mode
        = get_int_param_from_template_file(regular_opts.m_two_phase_mode,
                                           config_map);
    const auto shard_count
        = get_int_param_from_template_file(regular_opts.m_shard_count_key,
                                           config_map);
    const auto sentinel_count
        = get_int_param_from_template_file(regular_opts.m_sentinel_count_key,
                                           config_map);

    if(std::holds_alternative<size_t>(is_two_phase_mode)
       && std::get<size_t>(is_two_phase_mode) == 1) {
        output_filename = "2pc_generated_config.cfg";
        const auto coordinator_count = get_int_param_from_template_file(
            regular_opts.m_coordinator_count_key,
            config_map);
        if(std::holds_alternative<size_t>(sentinel_count)) {
            if(std::get<size_t>(sentinel_count) == 0) {
                return "Two-phase mode requires at least one configured "
                       "sentinel";
            } else {
                create_2pc_component(regular_opts.m_sentinel_count_key,
                                     std::get<size_t>(sentinel_count));
            }
        }
        if(std::holds_alternative<size_t>(shard_count)) {
            if(std::get<size_t>(shard_count) == 0) {
                return "Two-phase mode requires at least one configured shard";
            } else {
                create_2pc_component(regular_opts.m_shard_count_key,
                                     std::get<size_t>(shard_count));
            }
        }
        if(std::holds_alternative<size_t>(coordinator_count)) {
            if(std::get<size_t>(coordinator_count) == 0) {
                return "Two-phase mode required at least one configured "
                       "coordinator";
            } else {
                create_2pc_component(regular_opts.m_coordinator_count_key,
                                     std::get<size_t>(coordinator_count));
            }
        }
    } else {
        output_filename = "atomizer_generated_config.cfg";
        const auto atomizer_count = get_int_param_from_template_file(
            regular_opts.m_atomizer_count_key,
            config_map);
        const auto archiver_count = get_int_param_from_template_file(
            regular_opts.m_archiver_count_key,
            config_map);
        const auto watchtower_count = get_int_param_from_template_file(
            regular_opts.m_watchtower_count_key,
            config_map);

        if(std::holds_alternative<size_t>(watchtower_count)) {
            if(std::get<size_t>(watchtower_count) == 0) {
                return "Atomizer mode requires at least one configured "
                       "watchtower";
            } else {
                create_atomizer_component(regular_opts.m_watchtower_count_key,
                                          std::get<size_t>(watchtower_count));
            }
        }
        if(std::holds_alternative<size_t>(archiver_count)) {
            if(std::get<size_t>(archiver_count) == 0) {
                return "Atomizer mode requires at least one configured "
                       "archiver";
            } else {
                create_atomizer_component(regular_opts.m_archiver_count_key,
                                          std::get<size_t>(archiver_count));
            }
        }
        if(std::holds_alternative<size_t>(sentinel_count)) {
            if(std::get<size_t>(sentinel_count) == 0) {
                return "Sentinels require at least one configured shard";
            } else {
                create_atomizer_component(regular_opts.m_sentinel_count_key,
                                          std::get<size_t>(sentinel_count));
            }
        }
        if(std::holds_alternative<size_t>(atomizer_count)) {
            if(std::get<size_t>(atomizer_count) == 0) {
                return "Atomizer mode requires at least one configured "
                       "atomizer";
            } else {
                create_atomizer_component(regular_opts.m_atomizer_count_key,
                                          std::get<size_t>(atomizer_count));
            }
        }
        if(std::holds_alternative<size_t>(shard_count)) {
            create_atomizer_component(regular_opts.m_shard_count_key,
                                      std::get<size_t>(shard_count));
        }
    }
    write_options_to_config_file(output_filename);
    return "Success";
}

// load config by line
// Create all components and add to config string and save each count at
// the beginning of components Save out all tmpl_ stuff and add all other
// non tmpl_ stuff to config string Create method to deal with tmpl_ stuff
// Save out new config file

auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    if(args.size() < 2) {
        std::cerr << "Usage: " << args[0] << " <config template file>"
                  << std::endl;
        return 0;
    }

    auto cfg_or_err = generate_configuration_file(args[1]);
    return 0;
}
