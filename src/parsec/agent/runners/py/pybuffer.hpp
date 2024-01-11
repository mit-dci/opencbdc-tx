#ifndef OPENCBDC_TX_SRC_PARSEC_AGENT_PY_BUFFER_H_
#define OPENCBDC_TX_SRC_PARSEC_AGENT_PY_BUFFER_H_

#include "util/common/buffer.hpp"

#include <stdexcept>
#include <string>
#include <type_traits>

namespace cbdc::parsec::pybuffer {
    // An extension of the deafult cbdc::buffer class
    // Useful for writing function parameters in the "expected" format
    // such that they are readable by the Python runner
    class pyBuffer : public cbdc::buffer {
      public:
        /// Append a string to the buffer (with null-terminator)
        /// \param data String to append to buffer
        void appendString(const std::string& data);
        /// Append a C-style string to the buffer of given length
        /// \param data Data to append to buffer
        /// \param len Length of string
        void appendCStr(const char* data, size_t len);
        /// Append a byte vector to the buffer
        /// \param data Data to append to buffer
        void appendByteVector(const std::vector<std::byte>& data);

        /// End a "section" in the buffer. This is used to format the data
        /// such that the Python runner can read it.
        void endSection();

        /// Append some arithmetic value T to the buffer
        /// \param data Value to append to buffer
        template<
            typename T,
            typename
            = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
        void appendNumeric(const T data) {
            auto dataStr = std::to_string(data);
            this->append(dataStr.c_str(),
                         dataStr.size() + 1); // append the null terminator
        }
    };
}

#endif
