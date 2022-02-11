// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SERIALIZATION_SERIALIZER_H_
#define OPENCBDC_TX_SRC_SERIALIZATION_SERIALIZER_H_

#include <cstddef>

namespace cbdc {
    /// Interface for serializing objects into and out of raw bytes
    /// representations.
    class serializer {
      public:
        virtual ~serializer() = default;
        serializer(const serializer&) = delete;
        auto operator=(const serializer&) = delete;
        serializer(serializer&&) = delete;
        auto operator=(serializer&&) = delete;

        /// Indicates whether the last serialization operation succeeded.
        /// \return true if the last serialization succeeded.
        virtual explicit operator bool() const = 0;

        /// Moves the serialization cursor forward by the given number of
        /// bytes.
        ///
        /// \param len number of bytes to advance the cursor by.
        virtual void advance_cursor(size_t len) = 0;

        /// Resets the cursor to the start of the buffer.
        virtual void reset() = 0;

        /// Indicates whether the cursor is at or beyond the end of the buffer.
        /// \return true if the cursor is at or beyond the end of the buffer.
        [[nodiscard]] virtual auto end_of_buffer() const -> bool = 0;

        /// Attempts to write the given raw data into the buffer starting at
        /// the current cursor position.
        /// \param data pointer to the start of the data to write.
        /// \param len number of bytes of the data to write.
        /// \return true if the serializer wrote the requested number of bytes
        ///         to the buffer.
        virtual auto write(const void* data, size_t len) -> bool = 0;

        /// Attempts to read the requested number of bytes from the current
        /// cursor position into the given memory location.
        /// \param data memory destination into which to copy the data from the
        ///             buffer.
        /// \param len number of bytes to read from the buffer.
        /// \return true if the serializer read the requested number of bytes
        ///         from the buffer into the destination.
        virtual auto read(void* data, size_t len) -> bool = 0;

      protected:
        serializer() = default;
    };
}

#endif
