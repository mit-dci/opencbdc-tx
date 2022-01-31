// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SERIALIZATION_SIZE_SERIALIZER_H_
#define OPENCBDC_TX_SRC_SERIALIZATION_SIZE_SERIALIZER_H_

#include "serializer.hpp"

namespace cbdc {
    /// Utility class for determining the size of a buffer needed to serialize
    /// a sequence of objects. The class doesn't perform any actual
    /// serialization and just adds up the sizes. Deserialization is
    /// not supported and always fails to read any data.
    class size_serializer final : public serializer {
      public:
        size_serializer() = default;

        /// Indicates whether the last serialization operation succeeded.
        /// Serialization always succeeds for size serializer.
        /// \return true.
        explicit operator bool() const final;

        /// Increases the size counter by the given number of bytes.
        /// \param len number of bytes.
        void advance_cursor(size_t len) final;

        /// Resets the size counter to zero.
        void reset() final;

        /// Size serializer has no underlying buffer so this method always
        /// returns false.
        /// \return false.
        [[nodiscard]] auto end_of_buffer() const -> bool final;

        /// Increases size counter by the given number of bytes.
        /// \param data pointer is not read from.
        /// \param len number of bytes by which to increase the size counter.
        /// \return true.
        auto write(const void* data, size_t len) -> bool final;

        /// Read is not implemented for size serializer.
        /// \return false.
        auto read(void* data, size_t len) -> bool final;

        /// Returns the number of bytes accumulated in the size counter during
        /// mock serialization.
        /// \return number of bytes a buffer would need for serialization.
        [[nodiscard]] auto size() const -> size_t;

      private:
        size_t m_cursor{};
    };
}

#endif
