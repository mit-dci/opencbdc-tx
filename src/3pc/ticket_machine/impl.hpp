// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_TICKET_MACHINE_IMPL_H_
#define OPENCBDC_TX_SRC_3PC_TICKET_MACHINE_IMPL_H_

#include "interface.hpp"
#include "util/common/logging.hpp"

#include <atomic>
#include <memory>

namespace cbdc::threepc::ticket_machine {
    /// \brief Thread-safe ticket machine implementation.
    class impl : public interface {
      public:
        /// Constructor.
        /// \param logger log instance.
        /// \param range size of ticket number range to return.
        explicit impl(std::shared_ptr<logging::log> logger,
                      ticket_number_type range);

        /// Returns a new range of ticket numbers via the provided callback
        /// function. Calls the callback function before returning.
        /// \param result_callback function to call with result.
        /// \return true.
        auto get_ticket_number(get_ticket_number_callback_type result_callback)
            -> bool override;

      private:
        std::shared_ptr<logging::log> m_log;
        std::atomic<ticket_number_type> m_next_ticket_number{};
        ticket_number_type m_range;
    };
}

#endif
