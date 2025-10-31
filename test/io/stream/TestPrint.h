#pragma once

#include <exec/io/stream/Stream.h>

#include <supp/CircularBuffer.h>

#include <algorithm>

namespace exec::test {

struct Print : public exec::Print {
    virtual ~Print() = default;

    int availableForWrite() {
        return static_cast<int>(buf.capacity() - buf.size());
    }

    size_t write(uint8_t c) {
        return write(reinterpret_cast<const char*>(&c), 1);
    }

    size_t write(const char* b, size_t size) {
        size = std::min(size, buf.capacity() - buf.size());
        for (size_t i = 0; i < size; ++i) {
            buf.push(b[i]);  // NOLINT
        }
        return size;
    }

    supp::CircularBuffer<char, 10> buf;
};

}  // namespace exec::test
