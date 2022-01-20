// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SERIALIZATION_OSTREAM_SERIALIZER_H_
#define OPENCBDC_TX_SRC_SERIALIZATION_OSTREAM_SERIALIZER_H_

#include "stream_serializer.hpp"

#include <ostream>

namespace cbdc {
    /// \brief Implementation of \ref serializer for writing to a std::ostream.
    class ostream_serializer final : public stream_serializer {
      public:
        /// Constructor.
        /// \param s ostream to serialize into.
        explicit ostream_serializer(std::ostream& s);

        /// Indicates whether the serializer has reached the end of the stream.
        /// \return true if the ostream has reached the end of the stream.
        [[nodiscard]] auto end_of_buffer() const -> bool final;

        /// Moves the stream forward without writing to the stream.
        /// \param len number of bytes by which to move the stream forward.
        void advance_cursor(size_t len) final;

        /// Seeks the stream position to the beginning.
        void reset() final;

        /// Attempts to write the given number of bytes from the given memory
        /// location to the stream's current position.
        /// \param data memory location from which to write data into the
        ///             stream.
        /// \param len number of bytes to write into the stream.
        /// \return true if the operation wrote the requested number of bytes
        ///         from the data source into the stream.
        auto write(const void* data, size_t len) -> bool final;

        /// Not implemented for ostream.
        /// \return false.
        auto read(void* data, size_t len) -> bool final;

      private:
        std::ostream& m_str;

        using off_type = std::remove_reference_t<decltype(m_str)>::off_type;
    };
}

#endif
