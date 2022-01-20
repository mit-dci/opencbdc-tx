// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "console_logger.hpp"

namespace cbdc::raft {
    console_logger::console_logger(std::shared_ptr<cbdc::logging::log> log)
        : m_log(std::move(log)) {}

    void console_logger::put_details(int level,
                                     const char* source_file,
                                     const char* func_name,
                                     size_t line_number,
                                     const std::string& log_line) {
        const auto enum_level = static_cast<log_level>(level);
        switch(enum_level) {
            case log_level::trace:
                m_log->trace(source_file,
                             ":",
                             line_number,
                             func_name,
                             log_line);
                break;
            case log_level::debug:
                m_log->debug(source_file,
                             ":",
                             line_number,
                             func_name,
                             log_line);
                break;
            case log_level::info:
                m_log->info(source_file,
                            ":",
                            line_number,
                            func_name,
                            log_line);
                break;
            case log_level::warn:
                m_log->warn(source_file,
                            ":",
                            line_number,
                            func_name,
                            log_line);
                break;
            case log_level::error:
            case log_level::fatal:
                // Fatal calls std::exit, so make fatal errors a lower level
                // for nuraft.
                m_log->error(source_file,
                             ":",
                             line_number,
                             func_name,
                             log_line);
                break;
        }
    }

    auto console_logger::get_level() -> int {
        const auto ll = m_log->get_log_level();
        log_level level{};
        switch(ll) {
            case cbdc::logging::log_level::trace:
                level = log_level::trace;
                break;
            case cbdc::logging::log_level::debug:
                level = log_level::debug;
                break;
            case cbdc::logging::log_level::info:
                level = log_level::info;
                break;
            case cbdc::logging::log_level::warn:
                level = log_level::warn;
                break;
            // Demote fatal errors to avoid exiting.
            case cbdc::logging::log_level::fatal:
            case cbdc::logging::log_level::error:
                level = log_level::error;
                break;
        }

        return static_cast<int>(level);
    }
}
