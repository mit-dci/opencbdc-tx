// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
//                    MITRE Corporation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_CONFIG_TOOLS_CONFIG_GENERATOR_H_
#define OPENCBDC_TX_CONFIG_TOOLS_CONFIG_GENERATOR_H_

#include "util/common/config.hpp"
#include "util/common/random_source.hpp"

#include <random>
#include <secp256k1.h>

#define MAX_PORT_NUM 65535

// Structure to assist in creating shard id coverage
using ShardInfo = struct shard_info_struct {
    std::vector<size_t> coverage;
    size_t shard_id;
    size_t numbers_covered;
    double overlap_percentage_allowed;
    bool still_expanding;
    bool allow_overlap;
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
                         size_t _start_port);

        ~config_generator() = default;

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
        // Template file loaded to create configuration file from
        std::string& m_template_config_file;
        // Incrementing port to use in config file for all ports
        unsigned short m_current_port;
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
        void calculate_shard_coverage(size_t num_shards,
                                      bool randomize,
                                      size_t shard_size);
        // Helper for calculating shard id coverage
        void shard_bookkeeping(const std::vector<size_t>& array_total,
                               size_t shard_id);
        // Get random value based on mean and standard deviation
        [[nodiscard]] auto calculate_normal_distribution_point(size_t mean,
                                                               double std_dev,
                                                               bool randomize)
            -> double;
        // This is not a failsafe because we are simply generating a
        // configuration file here, however, it is still good to check that the
        // port is available
        [[nodiscard]] auto get_open_port() -> unsigned short;
        // Create Private/Public key pair repeatably (repeatable from run to
        // run)
        [[nodiscard]] auto create_repeatable_key_pair()
            -> std::pair<std::string, std::string>;
        // Create Private/Public key pair randomly (NOT repeatable from run to
        // run)
        [[nodiscard]] auto create_random_key_pair()
            -> std::pair<std::string, std::string>;
        // Create Private/Public key pair
        [[nodiscard]] auto create_key_pair(bool randomize)
            -> std::pair<std::string, std::string>;
        // Parse value as int, double or string from config file
        [[nodiscard]] auto parse_value(const std::string& value,
                                       bool keep_quotes) -> value_t;
        [[nodiscard]] auto get_param_from_template_file(
            std::string option,
            std::map<std::string, std::string>& config_map)
            -> std::variant<size_t, double, std::string>;
        void set_param_to_config_file(std::string key, std::string value);
        void set_param_to_config_file(std::string key, size_t value);
        void set_param_to_config_file(std::string key, double value);
        // Helper to set proper log level for either Two-Phase Commit or
        // Atomizer components
        void set_log_level(std::string key, std::string& log_level);
        // Method to create all the Two-Phase Commit related components for
        // generated config file
        void create_2pc_component(const char* type, size_t number);
        // Method to create all the Atomizer related components for generated
        // config file
        void create_atomizer_component(const char* type, size_t number);
        // Load template file from which we will generate the configuration
        // file.
        void load_template(std::string filename,
                           std::map<std::string, std::string>& config_map);
        // Write out the generated configuration file out to
        // <project_root>/build/config/tools
        void write_generated_config_to_file(const std::string& _config_file);
        // Copies the generated *.cfg file to the <project_root>/build
        // directory
        [[nodiscard]] auto copy_to_build_dir(std::string filename) -> bool;
        // Copies the *.tmpl file to the <project_root>/build/config/tools
        // directory
        void copy_templates_to_build_dir();
        template<typename T>
        [[maybe_unused]] auto find_value(std::string key, T& output) -> bool;
    };
}

#endif // OPENCBDC_TX_CONFIG_TOOLS_CONFIG_GENERATOR_H_
