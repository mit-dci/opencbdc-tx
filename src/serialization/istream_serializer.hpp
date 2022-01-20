// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SERIALIZATION_ISTREAM_SERIALIZER_H_
#define OPENCBDC_TX_SRC_SERIALIZATION_ISTREAM_SERIALIZER_H_

#include "stream_serializer.hpp"

#include <istream>

namespace cbdc {
    /// \brief Implementation of \ref serializer for reading from a
    /// std::istream.
    class istream_serializer final : public stream_serializer {
      public:
        /// Constructor.
        /// \param s istream to serialize from.
        explicit istream_serializer(std::istream& s);

        /// Indicates whether the serializer has reached the end of the stream.
        /// \return true if the istream has reached the end of the stream.
        [[nodiscard]] auto end_of_buffer() const -> bool final;

        /// Moves the stream forward without reading from the stream.
        /// \param len number of bytes by which to move the stream forward.
        void advance_cursor(size_t len) final;

        /// Seeks the stream position to the beginning.
        void reset() final;

        /// Not implemented for istream.
        /// \return false.
        auto write(const void* data, size_t len) -> bool final;

        /// Attempts to read the given number of bytes from the current
        /// position in the stream.
        /// \param data memory location to write the bytes.
        /// \param len number of bytes to read from the stream.
        /// \return true if the operation read the requested number of bytes
        ///         from the stream into the destination.
        auto read(void* data, size_t len) -> bool final;

      private:
        std::istream& m_str;

        using off_type = std::remove_reference_t<decltype(m_str)>::off_type;
    };
}

#endif
