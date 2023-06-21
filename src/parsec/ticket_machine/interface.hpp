// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_TICKET_MACHINE_INTERFACE_H_
#define OPENCBDC_TX_SRC_PARSEC_TICKET_MACHINE_INTERFACE_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <variant>

namespace cbdc::parsec::ticket_machine {
    /// Type alias for a ticket number.
    using ticket_number_type = uint64_t;

    /// \brief Ticket machine interface.
    ///
    /// Returns batches of monotonically increasing ticket numbers for
    /// identifying and sequencing transactions in the case of a conflict.
    class interface {
      public:
        virtual ~interface() = default;

        interface() = default;
        interface(const interface&) = delete;
        auto operator=(const interface&) -> interface& = delete;
        interface(interface&&) = delete;
        auto operator=(interface&&) -> interface& = delete;

        /// Error codes returned by the ticket machine.
        enum class error_code : uint8_t {
        };

        /// Return value from the ticket machine in the success case. An
        /// exclusive range of unique ticket numbers.
        using ticket_number_range_type
            = std::pair<ticket_number_type, ticket_number_type>;
        /// Return value from the ticket machine. Either a ticket number range
        /// or error code.
        using get_ticket_number_return_type
            = std::variant<ticket_number_range_type, error_code>;
        /// Callback function type for asynchronously handling ticket number
        /// requests.
        using get_ticket_number_callback_type
            = std::function<void(get_ticket_number_return_type)>;

        /// Asynchronously returns a new range of ticket numbers. Ticket
        /// numbers returned by this method must not repeat except in the
        /// case where all ticket numbers have been used, when ticket numbers
        /// will wrap around. \ref ticket_number_type should be large enough
        /// to make this a very rare occurance.
        /// \param result_callback function to call with ticket number range.
        /// \return true if the request was initiated successfully.
        virtual auto
        get_ticket_number(get_ticket_number_callback_type result_callback)
            -> bool
            = 0;
    };
}

#endif
