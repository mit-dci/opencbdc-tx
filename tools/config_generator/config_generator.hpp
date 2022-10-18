// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
//                    MITRE Corporation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_CONFIG_TOOLS_CONFIG_GENERATOR_H_
#define OPENCBDC_TX_CONFIG_TOOLS_CONFIG_GENERATOR_H_

#include "util/common/config.hpp"
#include "util/common/random_source.hpp"

#include <filesystem>
#include <random>
#include <secp256k1.h>

static constexpr auto MAX_PORT_NUM = 65535;

// Structure to assist in creating shard id coverage
using ShardInfo = struct shard_info_struct {
    std::vector<size_t> coverage;
    size_t shard_id{0};
    size_t numbers_covered{0};
    double overlap_percentage_allowed{0};
    bool still_expanding{true};
    bool allow_overlap{true};
    std::pair<size_t, size_t> current_coverage_expansion_limits;
};

using value_t = std::variant<std::string, size_t, double>;

namespace cbdc::generate_config {
    /// \brief Config_generator implementation.
    ///
    /// This class takes in a template configuration file, denoted by the
    /// ending "*.tmpl" and produces are usable (*.cfg) configuration file. The
    /// purpose of this class is to allow the user to create more complicated
    /// testing scenarios by removing some amount of manual effort when
    /// creating configurations.
    class config_generator {
      public:
        /// Constructor.
        ///
        /// \param _template_config_file The template configuration file from which
        /// the larger more intricate configuration file will be generated.
        /// \param _start_port Port to begin using and incrementing from for generated
        /// configuration file's endpoints
        config_generator(std::string& _template_config_file,
                         size_t _start_port,
                         std::string _build_dir);

        /// \brief generate_configuration_file
        /// Main workhorse method of this class. This method will generate a
        /// usable configuration file
        ///
        /// \return a string with all the error/warning/success messages, if any,
        /// that were produced while executing
        [[maybe_unused]] auto generate_configuration_file() -> std::string;

      private:
        // Boolean that tells us if file is valid or not
        bool template_file_is_valid;
        // Whether to randomize things that can be randomized
        bool m_randomize{false};
        // Template file loaded to create configuration file from
        std::string& m_template_config_file;
        // Incrementing port to use in config file for all ports
        unsigned short m_current_port;
        // Build directory
        std::filesystem::path m_build_dir;
        // Project root directory
        std::filesystem::path m_project_root_dir;
        // Map with shard ranges (shard_id, (start range, end_range)
        std::vector<ShardInfo> shard_info;
        std::default_random_engine generator;
        static const inline auto m_random_source
            = std::make_unique<cbdc::random_source>(
                cbdc::config::random_source);
        static const inline auto m_secp
            = std::unique_ptr<secp256k1_context,
                              decltype(&secp256k1_context_destroy)>(
                secp256k1_context_create(SECP256K1_CONTEXT_SIGN),
                &secp256k1_context_destroy);

        // Where the newly created configuration parameters will be stored as
        // we go along generating them
        std::stringstream m_new_config;

        std::map<std::string, value_t> template_options;

        // Calculate shard coverage
        void calculate_shard_coverage(size_t num_shards, size_t shard_size);
        // Helper for calculating shard id coverage
        void shard_bookkeeping(const std::vector<size_t>& array_total,
                               size_t shard_id);
        // Get random value based on mean and standard deviation
        [[nodiscard]] auto calculate_normal_distribution_point(size_t mean,
                                                               double std_dev)
            -> double;
        // This is not a failsafe because we are simply generating a
        // configuration file here, however, it is still good to check that the
        // port is available
        [[nodiscard]] auto get_open_port() -> unsigned short;
        // Create Private/Public key pair repeatably (repeatable from run to
        // run)
        [[nodiscard]] static auto create_repeatable_key_pair()
            -> std::pair<std::string, std::string>;
        // Create Private/Public key pair randomly (NOT repeatable from run to
        // run)
        [[nodiscard]] static auto create_random_key_pair()
            -> std::pair<std::string, std::string>;
        // Create Private/Public key pair
        [[nodiscard]] auto create_key_pair() const
            -> std::pair<std::string, std::string>;
        // Parse value as int, double or string from config file
        [[nodiscard]] static auto parse_value(const std::string& value,
                                              bool keep_quotes) -> value_t;
        [[nodiscard]] static auto get_param_from_template_file(
            const std::string& option,
            std::map<std::string, std::string>& config_map)
            -> std::variant<std::string, size_t, double>;
        void set_param_to_config_file(const std::string& key,
                                      const std::string& value);
        void set_param_to_config_file(const std::string& key, double value);
        void set_param_to_config_file(const std::string& key, size_t value);
        // Helper to set proper log level for either Two-Phase Commit or
        // Atomizer components
        void set_log_level(const std::string& key, std::string& log_level);
        // Method to create all the related components for generated config
        // file
        [[maybe_unused]] auto create_component(const char* type,
                                               size_t component_count,
                                               bool create_2pc) -> std::string;
        void create_2pc_shard(const std::string& default_log_level,
                              size_t current_component_num);
        void create_2pc_sentinel(const std::string& default_log_level,
                                 size_t current_component_num);
        void create_2pc_coordinator(const std::string& default_log_level,
                                    size_t current_component_num);
        void create_atomizer_shard(const std::string& default_log_level,
                                   size_t current_component_num);
        void create_atomizer_sentienl(const std::string& default_log_level,
                                      size_t current_component_num);
        void create_atomizer_archiver(const std::string& default_log_level,
                                      size_t current_component_num);
        void create_atomizer_atomizer(const std::string& default_log_level,
                                      size_t current_component_num);
        void create_atomizer_watchtower(const std::string& default_log_level,
                                        size_t current_component_num);
        // Load template file from which we will generate the configuration
        // file.
        static void
        load_template(const std::string& filename,
                      std::map<std::string, std::string>& config_map);
        // Write out the generated configuration file
        void write_generated_config_to_file(const std::string& _config_file);
        // Copies the generated *.cfg file to the <project_root>/build
        // directory
        [[nodiscard]] auto copy_to_build_dir(const std::string& filename)
            -> bool;
        void copy_templates_to_build_dir();
        template<typename T>
        [[nodiscard]] auto find_value(const std::string& key)
            -> std::optional<T>;
    };
}

#endif // OPENCBDC_TX_CONFIG_TOOLS_CONFIG_GENERATOR_H_
