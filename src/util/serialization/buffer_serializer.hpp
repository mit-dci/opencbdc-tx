// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SERIALIZATION_BUFFER_SERIALIZER_H_
#define OPENCBDC_TX_SRC_SERIALIZATION_BUFFER_SERIALIZER_H_

#include "serializer.hpp"
#include "util/common/buffer.hpp"

namespace cbdc {
    /// \brief Serializer implementation for \ref buffer.
    class buffer_serializer final : public cbdc::serializer {
      public:
        /// Constructor.
        /// \param pkt buffer to serialize into or out of.
        explicit buffer_serializer(cbdc::buffer& pkt);

        /// Indicates whether the last serialization operation succeeded.
        /// \return true if the last serialization operation succeeded.
        explicit operator bool() const final;

        /// Moves the cursor forward by the given number of bytes.
        /// \param len number of bytes by which to move the cursor forward.
        void advance_cursor(size_t len) final;

        /// Resets the cursor to the start of the buffer.
        void reset() final;

        /// Indicates whether the cursor is at or beyond the end of the buffer.
        /// \return true if the cursor is at or beyond the end of the buffer.
        [[nodiscard]] auto end_of_buffer() const -> bool final;

        /// Write the given bytes into the buffer from the current cursor
        /// position. If the data extends beyond the end of the buffer, expands
        /// the buffer size to fit the new data.
        /// \param data pointer to the start of the bytes to write.
        /// \param len number of bytes to write.
        /// \return true if the serializer wrote the entirety of the data to
        ///         the buffer.
        auto write(const void* data, size_t len) -> bool final;

        /// Read the given number of bytes from the buffer from the current
        /// cursor position into the given memory location.
        /// \param data pointer to the destination of the read data.
        /// \param len number of bytes to read from the buffer.
        /// \return true if the serializer read the requested number of bytes
        ///         from the buffer.
        auto read(void* data, size_t len) -> bool final;

      private:
        cbdc::buffer& m_pkt;
        size_t m_cursor{};
        bool m_valid{true};
    };
}

#endif
