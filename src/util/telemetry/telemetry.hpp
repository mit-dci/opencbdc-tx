// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COMMON_TELEMETRY_H_
#define OPENCBDC_TX_SRC_COMMON_TELEMETRY_H_

#include "util/common/buffer.hpp"
#include "util/common/hash.hpp"
#include "util/serialization/ostream_serializer.hpp"
#include "util/serialization/serializer.hpp"

#include <chrono>
#include <evmc/evmc.hpp>
#include <fstream>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

namespace cbdc {
    // telemetry_key can be used for identifying the measurement/function of
    // a telemetry point or the keys in the key-value map with telemetry
    // details. The final storage is a uint16_t allowing 65k different keys
    // which should suffice plenty. If you pass a string as key, it will be
    // inserted in an internal dictionary which is eventually written to the
    // end of the telemetry file to allow consumers to understand it while
    // trying to retain a more compact storage format (as opposed to writing
    // the string-based keys into the file all the time)
    using telemetry_key = std::variant<uint16_t, std::string>;

    // a telemetry value can be any of these types
    using telemetry_value = std::variant<int64_t,
                                         std::string,
                                         cbdc::hash_t,
                                         uint8_t,
                                         uint64_t,
                                         cbdc::buffer>;

    // telemetry_details is a key/value map using the key and value types
    // defined above
    using telemetry_details
        = std::vector<std::pair<telemetry_key, telemetry_value>>;

    // compact_telemetry has a fixed uint16_t type for the key part - meaning
    // it has been translated to the uint16_t in the dictionary
    using compact_telemetry_details
        = std::vector<std::pair<uint16_t, telemetry_value>>;

    template<class>
    inline constexpr bool always_false_v = false;

    // Pre-defined telemetry keys. It's faster to use these since you don't
    // have to allocate and pass around strings - but since telemetry_key is a
    // variant, you can add telemetry statements ad-hoc with a string value,
    // without necessarily defining it here first.
    // Make sure to add these to the initializer of m_keys above as well.
    enum telemetry_keys : uint16_t {
        txid = 0,
        ticket_number = 1,
        outcome = 2,
        latency = 3,
        address = 4,
        address2 = 5,
        storagekey = 6,
        storagevalue = 7,
        codeoffset = 8,
        locktype = 9,
        ticket_number2 = 10,
        storagekey2 = 11,
    };

    /// Keeps a collection of samples in memory to write to disk
    /// periodically in a separate thread - not to interfere with the
    /// code paths being measured
    class telemetry {
      public:
        telemetry(const telemetry&) = delete;
        auto operator=(const telemetry&) -> telemetry& = delete;

        telemetry(telemetry&&) = delete;
        auto operator=(telemetry&&) -> telemetry& = delete;

        // Constructs a new telemetry logger with the given output_name as
        // file to write to
        /// \param output_name The file name to output the telemetry log to
        explicit telemetry(const std::string& output_name);

        /// Returns the current time in nanoseconds since epoch
        static auto nano_now() -> int64_t;

        ~telemetry();

        /// Adds the given telemetry event to the collection
        /// \param measurement the type of event or measurement.
        /// \param details The details to associate with the event.
        void log(const telemetry_key& measurement,
                 const telemetry_details& details);

        /// Adds the given telemetry event to the collection at a particular
        /// time
        /// \param measurement The event that happened.
        /// \param details The details to associate with the event.
        /// \param time Time of the event
        void log(const telemetry_key& measurement,
                 const telemetry_details& details,
                 int64_t time);

      private:
        // Reads an environment variable and returns an empty string if it
        // is not present
        /// \param key The key to read from the environment
        static auto from_env(const char* key) -> std::string;

        // Either returns the uint16_t value from the telemetry key
        // variant OR retrieves the corresponding uint16_t from the m_keys
        // map based on the std::string value - OR inserts it if it doesn't
        // exist yet
        /// \param key The telemetry key to translate to uint16_t
        auto get_key(const telemetry_key& key) -> uint16_t;

        // Will loop over all telemetry_keys in a set of
        // telemetry_details and replace the key value with a uint16_t
        // using get_key
        /// \param det The telemetry details to translate to
        /// compact_telemetry_details
        auto to_compact(const telemetry_details& det)
            -> compact_telemetry_details;

        std::mutex m_mut;
        std::ofstream m_output_stream;
        cbdc::ostream_serializer m_ser;
        bool m_closed{false};

        // m_keys is written to the telemetry file trailer so that a consumer
        // of the telemetry file can get the translation from uint16 to the
        // defined name of a key from the file itself, and we don't have to
        // keep this table in sync with the consumers of the telemetry data all
        // the time.
        std::unordered_map<std::string, uint16_t> m_keys{
            {"txid", telemetry_keys::txid},
            {"ticket_number", telemetry_keys::ticket_number},
            {"outcome", telemetry_keys::outcome},
            {"latency", telemetry_keys::latency},
            {"address", telemetry_keys::address},
            {"address2", telemetry_keys::address2},
            {"storagekey", telemetry_keys::storagekey},
            {"storagevalue", telemetry_keys::storagevalue},
            {"codeoffset", telemetry_keys::codeoffset},
            {"locktype", telemetry_keys::locktype},
            {"ticket_number2", telemetry_keys::ticket_number2},
            {"storagekey2", telemetry_keys::storagekey2}};

        uint16_t m_next_key = 0;
    };

}

#endif // OPENCBDC_TX_SRC_COMMON_TELEMETRY_H_
