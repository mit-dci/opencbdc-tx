// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef OPENCBDC_TX_SRC_COMMON_LOGGING_H_
#define OPENCBDC_TX_SRC_COMMON_LOGGING_H_

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>

namespace cbdc::logging {
    /// No-op stream destination for log output.
    class null_stream : public std::ostream {
      public:
        /// Constructor. Sets the instance's stream buffer to nullptr.
        null_stream();

        template<typename T>
        auto operator<<(const T& /* unused */) -> null_stream& {
            return *this;
        }
    };

    /// Set of possible log levels. Used to configure \ref log. Each level
    /// implies that the logger should output messages at that level or
    /// greater.
    enum class log_level : uint8_t {
        /// Fine-grained, fully verbose operating information.
        trace,
        /// Diagnostic information.
        debug,
        /// General information about the state of the system.
        info,
        /// Potentially unintended, unexpected, or undesirable behavior
        warn,
        /// Serious, critical errors.
        error,
        /// Only fatal errors.
        fatal
    };

    /// Generalized logging class. Supports logging to stdout or an output file
    /// at a specified log level.
    class log {
      public:
        /// \brief Creates a new log instance.
        ///
        /// By default, logs to stdout and a \ref null_stream.
        /// \param level the log level (and above) to print to the logger(s).
        /// \param use_stdout indicates if the logger should print to stdout.
        /// \param logfile a pointer to a logfile stream.
        explicit log(log_level level,
                     bool use_stdout = true,
                     std::unique_ptr<std::ostream> logfile
                     = std::make_unique<null_stream>());

        /// Enables or disables printing the log output to stdout.
        /// \param stdout_enabled true if the log should print to stdout.
        void set_stdout_enabled(bool stdout_enabled);

        /// Changes the logfile output to another destination.
        /// \param logfile the stream to which to write log output.
        void set_logfile(std::unique_ptr<std::ostream> logfile);

        /// Changes the log level threshold.
        /// \param level anything for this log level and more severe will
        ///              be logged to the configured outputs.
        void set_loglevel(log_level level);

        /// Flushes the log buffer.
        static void flush();

        /// Writes the argument list to the trace log level.
        template<typename... Targs>
        void trace(Targs&&... args) {
            write_log_statement(log_level::trace,
                                std::forward<Targs>(args)...);
        }

        /// Writes the argument list to the debug log level.
        template<typename... Targs>
        void debug(Targs&&... args) {
            write_log_statement(log_level::debug,
                                std::forward<Targs>(args)...);
        }

        /// Writes the argument list to the info log level.
        template<typename... Targs>
        void info(Targs&&... args) {
            write_log_statement(log_level::info, std::forward<Targs>(args)...);
        }

        /// Writes the argument list to the warn log level.
        template<typename... Targs>
        void warn(Targs&&... args) {
            write_log_statement(log_level::warn, std::forward<Targs>(args)...);
        }

        /// Writes the argument list to the error log level.
        template<typename... Targs>
        void error(Targs&&... args) {
            write_log_statement(log_level::error,
                                std::forward<Targs>(args)...);
        }

        /// Writes the argument list to the fatal log level. Calls exit to
        /// terminate the program.
        template<typename... Targs>
        [[noreturn]] void fatal(Targs&&... args) {
            write_log_statement(log_level::fatal,
                                std::forward<Targs>(args)...);
            exit(EXIT_FAILURE);
        }

        /// Returns the current log level of the logger.
        /// \returns the current log level.
        [[nodiscard]] auto get_log_level() const -> log_level;

      private:
        bool m_stdout{true};
        log_level m_loglevel{};
        std::mutex m_stream_mut{};
        std::unique_ptr<std::ostream> m_logfile;

        auto static to_string(log_level level) -> std::string;
        static void write_log_prefix(std::stringstream& ss, log_level level);
        template<typename... Targs>
        void write_log_statement(log_level level, Targs&&... args) {
            if(m_loglevel <= level) {
                std::stringstream ss;
                write_log_prefix(ss, level);
                ((ss << " " << args), ...);
                ss << "\n";
                auto formatted_statement = ss.str();
                const std::lock_guard<std::mutex> lock(m_stream_mut);
                if(m_stdout) {
                    std::cout << formatted_statement;
                }
                *m_logfile << formatted_statement;
            }
        }
    };

    /// \brief Parses a capitalized string into a log level.
    ///
    /// Possible input values: TRACE, DEBUG, INFO, WARN, ERROR, and FATAL.
    /// \param level string corresponding to a log level.
    /// \return the log level, or std::nullopt if the input does not correspond
    ///         to a known log level.
    auto parse_loglevel(const std::string& level) -> std::optional<log_level>;
}

#endif // OPENCBDC_TX_SRC_COMMON_LOGGING_H_
