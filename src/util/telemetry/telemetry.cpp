// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "telemetry.hpp"

#include "format.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/ostream_serializer.hpp"

#include <type_traits>
#include <vector>

namespace cbdc {
    telemetry::telemetry(const std::string& output_name)
        : m_output_stream(output_name, std::ios_base::binary),
          m_ser(m_output_stream),
          m_next_key(static_cast<uint16_t>(m_keys.size())) {
        // Write a few environment variables into the header to provide
        // metadata to the telemetry file by tagging it with the test run
        // AWS instance/region and the role that we are playing in the
        // system.
        m_ser << to_compact(telemetry_details{
            {"testrun_id", telemetry::from_env("TESTRUN_ID")},
            {"testrun_role", telemetry::from_env("TESTRUN_ROLE")},
            {"aws_instance", telemetry::from_env("EC2_INSTANCE_ID")},
            {"aws_region", telemetry::from_env("AWS_REGION")}});
    }

    telemetry::~telemetry() {
        std::unique_lock<std::mutex> lck(m_mut);
        m_closed = true;
        long trailer_offset = m_output_stream.tellp();
        m_ser << m_keys;
        m_ser << trailer_offset;
        m_output_stream.close();
    }

    auto telemetry::get_key(const telemetry_key& key) -> uint16_t {
        if(std::holds_alternative<uint16_t>(key)) {
            auto keyuint = std::get<uint16_t>(key);
            assert(keyuint < m_next_key);
            return keyuint;
        }

        if(std::holds_alternative<std::string>(key)) {
            auto keystring = std::get<std::string>(key);
            auto [it, inserted] = m_keys.insert({keystring, m_next_key});
            if(inserted) {
                m_next_key++;
            }
            return it->second;
        }
        // This should never be reached
        return std::numeric_limits<uint16_t>::max();
    }

    auto telemetry::from_env(const char* key) -> std::string {
        if(const auto* env_v = std::getenv(key)) {
            return env_v;
        }
        return "";
    }

    auto telemetry::nano_now() -> int64_t {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::high_resolution_clock::now()
                       .time_since_epoch())
            .count();
    }

    auto telemetry::to_compact(const telemetry_details& det)
        -> compact_telemetry_details {
        auto cmp_det = compact_telemetry_details{};
        for(const auto& it : det) {
            cmp_det.emplace_back(get_key(it.first), it.second);
        }
        return cmp_det;
    }

    void telemetry::log(const telemetry_key& measurement,
                        const telemetry_details& details) {
        log(measurement, details, telemetry::nano_now());
    }

    void telemetry::log(const telemetry_key& measurement,
                        const telemetry_details& details,
                        int64_t time) {
        std::unique_lock<std::mutex> lck(m_mut);
        if(m_closed) {
            std::cerr << "Called log on a closed telemetry logger"
                      << std::endl;
            return;
        }
        m_ser << get_key(measurement) << to_compact(details) << time;
    }
}
