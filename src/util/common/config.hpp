// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
//               2022 MITRE Corporation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * \file config.hpp
 * Tools for reading options from a configuration file and building
 * application-specific parameter sets for use in executables.
 */

#ifndef OPENCBDC_TX_SRC_COMMON_CONFIG_H_
#define OPENCBDC_TX_SRC_COMMON_CONFIG_H_

#include "hash.hpp"
#include "hashmap.hpp"
#include "keys.hpp"
#include "logging.hpp"
#include "util/network/socket.hpp"

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace cbdc::config {
    /// Random source to use when generating keys.
    static constexpr const char* random_source{"/dev/urandom"};

    /// Human readable part (HRP) to use when encoding Bech32 addresses.
    static const std::string bech32_hrp = "usd";

    /// Symbol to use when printing currency values.
    static constexpr const char* currency_symbol{"$"};

    /// \brief Maximum bytes optimistically reserved at once during deserialization.
    /// When deserializing, we want to limit the amount of memory we reserve
    /// without the sender actually sending that amount of information. This
    /// constant is used when deserializing so that a sender must send at least
    /// X bytes of information for us to allocate X+1MiB of memory.
    static constexpr uint64_t maximum_reservation
        = static_cast<uint64_t>(1024 * 1024); // 1MiB

    namespace defaults {
        static constexpr size_t stxo_cache_depth{1};
        static constexpr size_t window_size{10000};
        static constexpr size_t shard_completed_txs_cache_size{10000000};
        static constexpr size_t batch_size{2000};
        static constexpr size_t target_block_interval{250};
        static constexpr int32_t election_timeout_upper_bound{4000};
        static constexpr int32_t election_timeout_lower_bound{2000};
        static constexpr int32_t heartbeat{1000};
        static constexpr int32_t raft_max_batch{100000};
        static constexpr size_t coordinator_max_threads{75};
        static constexpr size_t initial_mint_count{20000};
        static constexpr size_t initial_mint_value{100};
        static constexpr size_t watchtower_block_cache_size{100};
        static constexpr size_t watchtower_error_cache_size{1000000};
        static constexpr bool wait_for_followers{true};
        static constexpr size_t input_count{2};
        static constexpr size_t output_count{2};
        static constexpr double fixed_tx_rate{1.0};
        static constexpr size_t attestation_threshold{1};

        static constexpr auto log_level = logging::log_level::warn;
    }

    static constexpr auto endpoint_postfix = "endpoint";
    static constexpr auto loglevel_postfix = "loglevel";
    static constexpr auto raft_endpoint_postfix = "raft_endpoint";
    static constexpr auto stxo_cache_key = "stxo_cache_depth";
    static constexpr auto shard_count_key = "shard_count";
    static constexpr auto shard_prefix = "shard";
    static constexpr auto seed_privkey = "seed_privkey";
    static constexpr auto seed_value = "seed_value";
    static constexpr auto seed_from = "seed_from";
    static constexpr auto seed_to = "seed_to";
    static constexpr auto atomizer_prefix = "atomizer";
    static constexpr auto sentinel_count_key = "sentinel_count";
    static constexpr auto sentinel_prefix = "sentinel";
    static constexpr auto config_separator = "_";
    static constexpr auto db_postfix = "db";
    static constexpr auto start_postfix = "start";
    static constexpr auto end_postfix = "end";
    static constexpr auto atomizer_count_key = "atomizer_count";
    static constexpr auto archiver_prefix = "archiver";
    static constexpr auto batch_size_key = "batch_size";
    static constexpr auto window_size_key = "window_size";
    static constexpr auto target_block_interval_key = "target_block_interval";
    static constexpr auto election_timeout_upper_key
        = "election_timeout_upper";
    static constexpr auto election_timeout_lower_key
        = "election_timeout_lower";
    static constexpr auto heartbeat_key = "heartbeat";
    static constexpr auto snapshot_distance_key = "snapshot_distance";
    static constexpr auto raft_batch_size_key = "raft_max_batch";
    static constexpr auto input_count_key = "loadgen_sendtx_input_count";
    static constexpr auto output_count_key = "loadgen_sendtx_output_count";
    static constexpr auto invalid_rate_key = "loadgen_invalid_tx_rate";
    static constexpr auto fixed_tx_rate_key = "loadgen_fixed_tx_rate";
    static constexpr auto archiver_count_key = "archiver_count";
    static constexpr auto watchtower_count_key = "watchtower_count";
    static constexpr auto watchtower_prefix = "watchtower";
    static constexpr auto watchtower_client_ep_postfix = "client_endpoint";
    static constexpr auto watchtower_internal_ep_postfix = "internal_endpoint";
    static constexpr auto watchtower_block_cache_size_key
        = "watchtower_block_cache_size";
    static constexpr auto watchtower_error_cache_size_key
        = "watchtower_error_cache_size";
    static constexpr auto two_phase_mode = "2pc";
    static constexpr auto count_postfix = "count";
    static constexpr auto readonly = "readonly";
    static constexpr auto coordinator_prefix = "coordinator";
    static constexpr auto coordinator_count_key = "coordinator_count";
    static constexpr auto coordinator_max_threads = "coordinator_max_threads";
    static constexpr auto initial_mint_count_key = "initial_mint_count";
    static constexpr auto initial_mint_value_key = "initial_mint_value";
    static constexpr auto minter_count_key = "minter_count";
    static constexpr auto minter_prefix = "minter";

    static constexpr auto loadgen_count_key = "loadgen_count";
    static constexpr auto shard_completed_txs_cache_size
        = "shard_completed_txs_cache_size";
    static constexpr auto wait_for_followers_key = "wait_for_followers";
    static constexpr auto private_key_postfix = "private_key";
    static constexpr auto public_key_postfix = "public_key";
    static constexpr auto attestation_threshold_key = "attestation_threshold";

    /// [start, end] inclusive.
    using shard_range_t = std::pair<uint8_t, uint8_t>;

    /// Project-wide configuration options.
    struct options {
        /// Depth of the spent transaction cache in the atomizer, in blocks.
        size_t m_stxo_cache_depth{defaults::stxo_cache_depth};
        /// Maximum number of unconfirmed transactions in atomizer-cli.
        size_t m_window_size{defaults::window_size};
        /// Number of inputs in fixed-size transactions from atomizer-cli.
        size_t m_input_count{defaults::input_count};
        /// Number of outputs in fixed-size transactions from atomizer-cli.
        size_t m_output_count{defaults::output_count};
        /// Proportion of invalid transactions sent from atomizer-cli.
        double m_invalid_rate{0.0};
        /// Proportion of fixed transactions sent from atomizer-cli.
        double m_fixed_tx_rate{defaults::fixed_tx_rate};
        /// The number of completed transactions that each locking shard (2PC)
        /// keeps in memory for responding to queries through the read-only
        /// endpoint.
        size_t m_shard_completed_txs_cache_size{
            defaults::shard_completed_txs_cache_size};

        /// List of atomizer endpoints, ordered by atomizer ID.
        std::vector<network::endpoint_t> m_atomizer_endpoints;
        /// List of archiver endpoints, ordered by archiver ID.
        std::vector<network::endpoint_t> m_archiver_endpoints;
        /// List of sentinel endpoints, ordered by sentinel ID.
        std::vector<network::endpoint_t> m_sentinel_endpoints;
        /// List of watchtower client endpoints, ordered by watchtower ID.
        std::vector<network::endpoint_t> m_watchtower_client_endpoints;
        /// List of watchtower internal endpoints, ordered by watchtower ID
        std::vector<network::endpoint_t> m_watchtower_internal_endpoints;
        /// List of shard endpoints, ordered by shard ID.
        std::vector<network::endpoint_t> m_shard_endpoints;
        /// List of atomizer raft endpoints, ordered by atomizer ID.
        std::vector<std::optional<network::endpoint_t>>
            m_atomizer_raft_endpoints;
        /// Maximum transaction batch size for one log entry in the raft
        /// atomizer or one batch in the coordinator.
        size_t m_batch_size{defaults::batch_size};
        /// Target block creation interval in the atomizer in milliseconds.
        size_t m_target_block_interval{defaults::target_block_interval};
        /// List of atomizer log levels by atomizer ID.
        std::vector<logging::log_level> m_atomizer_loglevels;
        /// Raft election timeout upper bound in milliseconds.
        int32_t m_election_timeout_upper{
            defaults::election_timeout_upper_bound};
        /// Raft election timeout lower bound in milliseconds.
        int32_t m_election_timeout_lower{
            defaults::election_timeout_lower_bound};
        /// Raft heartbeat timeout in milliseconds.
        int32_t m_heartbeat{defaults::heartbeat};
        /// Raft snapshot distance, in number of log entries.
        int32_t m_snapshot_distance{0};
        /// Maximum number of raft log entries to batch into one RPC message.
        int32_t m_raft_max_batch{defaults::raft_max_batch};
        /// List of shard log levels by shard ID.
        std::vector<logging::log_level> m_shard_loglevels;
        /// List of shard DB paths by shard ID.
        std::vector<std::string> m_shard_db_dirs;
        /// List of shard UHS ID ranges by shard ID. Each shard range is
        /// inclusive of the start and end of the range.
        std::vector<shard_range_t> m_shard_ranges;

        /// private key used for initial seed.
        std::optional<privkey_t> m_seed_privkey;
        /// output value to use for initial seed.
        size_t m_seed_value{0};
        /// starting index for faked input used for initial seed.
        size_t m_seed_from{0};
        /// ending index for faked input used for initial seed.
        size_t m_seed_to{0};

        /// List of sentinel log levels by sentinel ID.
        std::vector<logging::log_level> m_sentinel_loglevels;
        /// List of archiver log levels by archiver ID.
        std::vector<logging::log_level> m_archiver_loglevels;
        /// List of archiver log levels by watchtower ID.
        std::vector<logging::log_level> m_watchtower_loglevels;
        /// List of archiver DB paths by archiver ID.
        std::vector<std::string> m_archiver_db_dirs;
        /// Flag set if m_input_count or m_output_count are greater than zero.
        /// Causes the atomizer-cli to send fixed-size transactions.
        bool m_fixed_tx_mode{false};
        /// Flag set if the architecture is two-phase commit.
        bool m_twophase_mode{false};
        /// List of locking shard endpoints, ordered by shard ID then node ID.
        std::vector<std::vector<network::endpoint_t>>
            m_locking_shard_endpoints;
        /// List of locking shard raft endpoints, ordered by shard ID then node
        /// ID.
        std::vector<std::vector<network::endpoint_t>>
            m_locking_shard_raft_endpoints;
        /// List of locking shard read-only endpoints, ordered by shard ID then
        /// node ID.
        std::vector<std::vector<network::endpoint_t>>
            m_locking_shard_readonly_endpoints;
        /// List of coordinator endpoints, ordered by shard ID then node ID.
        std::vector<std::vector<network::endpoint_t>> m_coordinator_endpoints;
        /// List of coordinator raft endpoints, ordered by shard ID then node
        /// ID.
        std::vector<std::vector<network::endpoint_t>>
            m_coordinator_raft_endpoints;
        /// Coordinator thread count limit.
        size_t m_coordinator_max_threads{defaults::coordinator_max_threads};
        /// List of coordinator log levels, ordered by coordinator ID.
        std::vector<logging::log_level> m_coordinator_loglevels;

        /// Number of outputs in the initial mint transaction.
        size_t m_initial_mint_count{defaults::initial_mint_count};
        /// Value for all outputs in the initial mint transaction.
        size_t m_initial_mint_value{defaults::initial_mint_value};

        /// Map of private keys for minters keyed by the index value
        /// in the configuration file.
        std::unordered_map<size_t, privkey_t> m_minter_private_keys;

        /// Set of public keys belonging to authorized minters
        std::unordered_set<pubkey_t, hashing::null> m_minter_public_keys;

        /// Number of blocks to store in watchtower block caches.
        /// (0=unlimited). Defaults to 1 hour of blocks.
        size_t m_watchtower_block_cache_size{
            defaults::watchtower_block_cache_size};
        /// Number of errors to store in watchtower error caches.
        /// (0=unlimited).
        size_t m_watchtower_error_cache_size{
            defaults::watchtower_error_cache_size};

        /// Number of load generators over which to split pre-seeded UTXOs.
        size_t m_loadgen_count{0};

        /// Flag for whether the raft leader should re-attempt to join
        /// followers to the cluster until successful.
        bool m_wait_for_followers{defaults::wait_for_followers};

        /// Private keys for sentinels.
        std::unordered_map<size_t, privkey_t> m_sentinel_private_keys;

        /// Public keys for sentinels.
        std::unordered_set<pubkey_t, hashing::null> m_sentinel_public_keys;

        /// Number of sentinel attestations needed for a compact transaction.
        size_t m_attestation_threshold{defaults::attestation_threshold};
    };

    /// Read options from the given config file without checking invariants.
    /// \param config_file the path to the config file from which to load
    ///                    options.
    /// \return options struct with all required values, or string with error
    ///         message on failure.
    auto read_options(const std::string& config_file)
        -> std::variant<options, std::string>;

    /// Loads options from the given config file and check for invariants.
    /// \param config_file the path to the config file from which load options.
    /// \return valid options struct, or string with error message on failure.
    auto load_options(const std::string& config_file)
        -> std::variant<options, std::string>;

    /// Checks a fully populated options struct for invariants. Assumes struct
    /// contains all required options.
    /// \param opts options struct to check.
    /// \return std::nullopt if the struct satisfies all invariants. Error
    ///         string otherwise.
    auto check_options(const options& opts) -> std::optional<std::string>;

    /// Checks if a hash is in the given range handled.
    /// \param range shard hash prefix range.
    /// \param val hash to check.
    /// \return true if the hash prefix is within the inclusive range.
    auto hash_in_shard_range(const shard_range_t& range, const hash_t& val)
        -> bool;

    /// Calculates the sub-range of total seeded outputs for a particular load
    /// generator ID.
    /// \param opts options struct from which to read seed data.
    /// \param gen_id ID of load generator for which to calculate the
    ///               sub-range. Must be less than the load generator count.
    /// \return pair representing start and end of the seed range.
    auto loadgen_seed_range(const options& opts, size_t gen_id)
        -> std::pair<size_t, size_t>;

    /// Converts c-args from an executable's main function into a vector of
    /// strings.
    auto get_args(int argc, char** argv) -> std::vector<std::string>;

    /// Reads configuration parameters line-by-line from a file. Expects a file
    /// of line-separated parameters with each line in the form key=value,
    /// where the key is a lower-case string that may contain numbers and
    /// symbols. Acceptable value types:
    /// - Strings: quoted with double quotes. Ex: some_string="hello"
    /// - Integers: standalone numbers. Ex: some_int=30
    /// - Doubles: a number with a decimal point. Ex: some_double=12.4
    /// - Log levels: in the form of a string. Must be one of the log levels
    ///   enumerated in logging.hpp, in upper-case. Ex: some_loglevel="TRACE"
    /// - Endpoints: strings in the form "hostname:port".
    ///
    /// The class will override file-enumerated config parameters with
    /// values from environment variables, where the environment variable key
    /// is the upper-case version of the config file string. For example, a
    /// window_size=40000 line in the config file would be overridden by
    /// setting the environment variable WINDOW_SIZE=50000. String options
    /// supplied through environment variables must be quoted, e.g.
    /// SOMEKEY='"some_value"'. See the example configuration files and
    /// README.md for more detail and valid options.
    class parser {
      public:
        /// Constructor.
        /// \param filename path to the config file to read.
        explicit parser(const std::string& filename);

        /// Constructor.
        /// \param stream the generic stream used to add config values.
        explicit parser(std::istream& stream);

        /// Returns the given key if its value is a string.
        /// \param key key to retrieve.
        /// \return value associated with the key or std::nullopt if the value
        ///         was not a string or does not exist.
        [[nodiscard]] auto get_string(const std::string& key) const
            -> std::optional<std::string>;

        /// Return the value for the given key if its value is a long.
        /// \param key key to retrieve.
        /// \return value associated with the key, or std::nullopt if the value
        ///         was not a long or doesn't exist.
        [[nodiscard]] auto get_ulong(const std::string& key) const
            -> std::optional<size_t>;

        /// Return the value for the given key if its value is an endpoint.
        /// \param key key to retrieve.
        /// \return value associated with the key, or std::nullopt if the value
        ///         was not a endpoint or does not exist.
        [[nodiscard]] auto get_endpoint(const std::string& key) const
            -> std::optional<network::endpoint_t>;

        /// Return the value for the given key if its value is a loglevel.
        /// \param key key to retrieve.
        /// \return value associated with the key, or std::nullopt if the value
        ///         was not a loglevel or does not exist.
        [[nodiscard]] auto get_loglevel(const std::string& key) const
            -> std::optional<logging::log_level>;

        /// Return the value for the given key if its value is a double.
        /// \param key key to retrieve.
        /// \return value associated with the key, or std::nullopt if the value
        ///         was not a double or does not exist.
        [[nodiscard]] auto get_decimal(const std::string& key) const
            -> std::optional<double>;

      private:
        using value_t = std::variant<std::string, size_t, double>;

        [[nodiscard]] auto find_or_env(const std::string& key) const
            -> std::optional<value_t>;

        template<typename T>
        [[nodiscard]] auto get_val(const std::string& key) const
            -> std::optional<T> {
            const auto it = find_or_env(key);
            if(it) {
                const auto* val = std::get_if<T>(&it.value());
                if(val != nullptr) {
                    return *val;
                }
            }

            return std::nullopt;
        }

        void init(std::istream& stream);

        [[nodiscard]] static auto parse_value(const std::string& val)
            -> value_t;

        std::map<std::string, value_t> m_options;
    };

    auto parse_ip_port(const std::string& in_str) -> network::endpoint_t;
}

#endif // OPENCBDC_TX_SRC_COMMON_CONFIG_H_
