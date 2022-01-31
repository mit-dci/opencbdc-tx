// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RAFT_CONSOLE_LOGGER_H_
#define OPENCBDC_TX_SRC_RAFT_CONSOLE_LOGGER_H_

#include "util/common/logging.hpp"

#include <libnuraft/nuraft.hxx>

namespace cbdc::raft {
    /// Map from NuRaft-internal log levels to names.
    enum class log_level : int {
        trace = 6,
        debug = 5,
        info = 4,
        warn = 3,
        error = 2,
        fatal = 1
    };

    /// nuraft::logger implementation using \ref logging::log.
    class console_logger : public nuraft::logger {
      public:
        /// Constructor.
        /// \param log log instance.
        explicit console_logger(std::shared_ptr<logging::log> log);

        /// Write a log message to the log.
        /// \param level severity of the message.
        /// \param source_file source file where the message originates.
        /// \param func_name function name where the message originates.
        /// \param line_number line number in the source file where the message
        ///                    originates.
        /// \param log_line log message.
        void put_details(int level,
                         const char* source_file,
                         const char* func_name,
                         size_t line_number,
                         const std::string& log_line) override;

        /// Return the log level of the underlying logger.
        /// \return log level.
        [[nodiscard]] auto get_level() -> int override;

      private:
        std::shared_ptr<logging::log> m_log;
    };
}

#endif // OPENCBDC_TX_SRC_RAFT_CONSOLE_LOGGER_H_
