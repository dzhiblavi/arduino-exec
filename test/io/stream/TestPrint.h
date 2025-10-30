#pragma once

#include <exec/io/stream/Stream.h>

#include <supp/CircularBuffer.h>

namespace exec::test {

struct Print : public exec::Print {
    virtual ~Print() = default;

    int availableForWrite() override {
        return static_cast<int>(buf.capacity() - buf.size());
    }

    size_t write(const char* b, size_t size) override {
        size = std::min(size, buf.capacity() - buf.size());
        for (size_t i = 0; i < size; ++i) {
            buf.push(b[i]);  // NOLINT
        }
        return size;
    }

    supp::CircularBuffer<char, 10> buf;
};

}  // namespace exec::test
