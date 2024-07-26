// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COMMON_BUFFER_H_
#define OPENCBDC_TX_SRC_COMMON_BUFFER_H_

#include <optional>
#include <string>
#include <vector>

namespace cbdc {
    /// Buffer to store and retrieve byte data.
    class buffer {
      public:
        buffer() = default;

        /// Returns the number of bytes contained in the buffer.
        /// \return the number of bytes.
        [[nodiscard]] auto size() const -> size_t;

        /// Returns a raw pointer to the start of the buffer data.
        /// \return a pointer to the data.
        [[nodiscard]] auto data() -> void*;

        /// Returns a raw pointer to the start of the buffer data.
        /// \return a pointer to the data.
        [[nodiscard]] auto data() const -> const void*;

        /// Returns a raw pointer to the start of the buffer data.
        /// \param offset the byte offset into the buffer.
        /// \return a pointer to the data.
        [[nodiscard]] auto data_at(size_t offset) -> void*;

        /// Returns a raw pointer to the start of the packet data.
        /// \param offset the byte offset into the packet.
        /// \return a pointer to the data.
        [[nodiscard]] auto data_at(size_t offset) const -> const void*;

        /// Adds the given number of bytes from the given pointer to the end of
        /// the buffer.
        /// \param data pointer to the start of the data.
        /// \param len the number of bytes to read.
        void append(const void* data, size_t len);

        /// Removes any existing content in the buffer making its size 0.
        void clear();

        auto operator==(const buffer& other) const -> bool;

        /// Extends the size of the buffer by the given length.
        /// \param len the number of bytes to add.
        void extend(size_t len);

        /// Returns a pointer to the data, cast to an unsigned char*.
        /// \return unsigned char pointer.
        [[nodiscard]] auto c_ptr() const -> const unsigned char*;

        /// Returns a pointer to the data, cast to a char*.
        /// \return char pointer.
        [[nodiscard]] auto c_str() const -> const char*;

        /// Creates a new buffer from the provided hex string.
        /// \param hex string-encoded hex representation of this buffer.
        /// \return a new buffer.
        static auto
        from_hex(const std::string& hex) -> std::optional<cbdc::buffer>;

        /// Returns a hex string representation of the contents of the buffer.
        /// \return a hex encoded string.
        [[nodiscard]] auto to_hex() const -> std::string;

        /// Creates a new buffer from the provided hex string optionally
        /// prefixed with a prefix sequence
        /// \param hex string-encoded hex representation of this buffer.
        /// \param prefix text at start of hex string. Defaults to "0x".
        /// \return a new buffer.
        static auto from_hex_prefixed(const std::string& hex,
                                      const std::string& prefix
                                      = "0x") -> std::optional<buffer>;

        /// Returns a hex string representation of the contents of the
        /// buffer prefixed with a prefix sequence
        /// \return a hex encoded string.
        [[nodiscard]] auto to_hex_prefixed(const std::string& prefix
                                           = "0x") const -> std::string;

      private:
        std::vector<std::byte> m_data{};
    };
}

#endif // OPENCBDC_TX_SRC_COMMON_BUFFER_H_
