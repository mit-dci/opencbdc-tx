#include "pybuffer.hpp"

#include <string>

namespace cbdc::parsec::pybuffer {
    void pyBuffer::appendString(const std::string& data) {
        this->append(data.c_str(), data.length() + 1);
    }

    void pyBuffer::appendCStr(const char* data, size_t len) {
        this->append(data, len);
        this->append("\0", 1);
    }

    void pyBuffer::appendByteVector(const std::vector<std::byte>& data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const auto* dataStr = reinterpret_cast<const char*>(data.data());
        this->append(dataStr, data.size());
        this->append("\0", 1);
    }

    void pyBuffer::endSection() {
        this->append("|", 2);
    }
}
