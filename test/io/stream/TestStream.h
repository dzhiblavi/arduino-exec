#pragma once

#include <exec/io/stream/Stream.h>

#include <supp/CircularBuffer.h>

namespace exec::test {

struct Stream : public ::exec::Stream {
    virtual ~Stream() = default;

    int available() override {
        return static_cast<int>(buf.size());
    }

    int read() override {
        return buf.empty() ? -1 : buf.pop();
    }

    int peek() override {
        return buf.front();
    }

    int availableForWrite() {
        return 0;
    }

    size_t write(uint8_t) {
        return 0;
    }

    size_t write(const char*, size_t) {
        return 0;
    }

    supp::CircularBuffer<int, 10> buf;
};

}  // namespace exec::test
