// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RAFT_SERIALIZATION_H_
#define OPENCBDC_TX_SRC_RAFT_SERIALIZATION_H_

#include "util/serialization/serializer.hpp"

#include <libnuraft/buffer.hxx>

namespace cbdc {
    /// \brief Implements \ref serializer for nuraft::buffer.
    ///
    /// NuRaft buffers contain a cursor which this class manipulates directly.
    class nuraft_serializer final : public cbdc::serializer {
      public:
        /// Constructor. Resets the buffer's internal cursor.
        /// \param buf nuraft::buffer to serialize into and out of.
        explicit nuraft_serializer(nuraft::buffer& buf);

        /// Bool operator for checking whether the last serialization operation
        /// completed successfully.
        /// \return false if the last serialization operation failed.
        explicit operator bool() const final;

        /// Moves the cursor inside the buffer forward by the given
        /// number of bytes. Does not change the size of the buffer.
        /// \param len number of bytes by which to advance the cursor.
        void advance_cursor(size_t len) final;

        /// Resets the cursor to the start of the buffer.
        void reset() final;

        /// Returns whether the cursor is currently at the end of the buffer.
        /// \return true if there are no more bytes to read or no more space
        ///         to write to the buffer.
        [[nodiscard]] auto end_of_buffer() const -> bool final;

        /// Writes the given raw bytes to the buffer from the current position
        /// of the cursor. Does not resize the buffer.
        /// \param data pointer to the start of the data to write.
        /// \param len number of bytes to write.
        /// \return true if the operation wrote the entirety of the data to the buffer.
        auto write(const void* data, size_t len) -> bool final;

        /// Reads the given number of bytes from the current cursor in the
        /// buffer into the given destination.
        /// \param data pointer to the destination for the data.
        /// \param len number of bytes to read from the buffer.
        /// \return true if operation successfully copied the requested number of bytes
        ///        from the buffer to the given destination.
        auto read(void* data, size_t len) -> bool final;

      private:
        nuraft::buffer& m_buf;
        bool m_valid{true};
    };
}

#endif // OPENCBDC_TX_SRC_RAFT_SERIALIZATION_H_
