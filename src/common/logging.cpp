// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "logging.hpp"

namespace cbdc::logging {
    null_stream::null_stream() : std::ostream(nullptr) {}

    log::log(log_level level,
             bool use_stdout,
             std::unique_ptr<std::ostream> logfile)
        : m_stdout(use_stdout),
          m_loglevel(level),
          m_logfile(std::move(logfile)) {}

    void log::set_stdout_enabled(bool stdout_enabled) {
        m_stdout = stdout_enabled;
    }

    void log::set_logfile(std::unique_ptr<std::ostream> logfile) {
        m_logfile = std::move(logfile);
    }

    void log::set_loglevel(log_level level) {
        m_loglevel = level;
    }

    auto log::get_log_level() const -> log_level {
        return m_loglevel;
    }

    auto log::to_string(log_level level) -> std::string {
        switch(level) {
            case log_level::trace:
                return "TRACE";
            case log_level::debug:
                return "DEBUG";
            case log_level::info:
                return "INFO ";
            case log_level::warn:
                return "WARN ";
            case log_level::error:
                return "ERROR";
            case log_level::fatal:
                return "FATAL";
            default:
                return "NONE ";
        }
    }

    void log::write_log_prefix(std::stringstream& ss, log_level level) {
        auto now = std::chrono::system_clock::now();
        auto now_t = std::chrono::system_clock::to_time_t(now);
        auto now_ms
            = std::chrono::time_point_cast<std::chrono::milliseconds>(now);

        static constexpr int msec_per_sec = 1000;
        auto const now_ms_f = now_ms.time_since_epoch().count() % msec_per_sec;
        ss << std::put_time(std::localtime(&now_t), "[%Y-%m-%d %H:%M:%S.")
           << std::setfill('0') << std::setw(3) << now_ms_f << "] ["
           << to_string(level) << "]";
    }

    void log::flush() {
        std::cout << std::flush;
    }

    auto parse_loglevel(const std::string& level) -> std::optional<log_level> {
        if(level == "TRACE") {
            return log_level::trace;
        }
        if(level == "DEBUG") {
            return log_level::debug;
        }
        if(level == "INFO") {
            return log_level::info;
        }
        if(level == "WARN") {
            return log_level::warn;
        }
        if(level == "ERROR") {
            return log_level::error;
        }
        if(level == "FATAL") {
            return log_level::fatal;
        }
        return std::nullopt;
    }
}
