#pragma once

#if !__has_include(<Stream.h>)

#include <cstdint>

class Stream {
 public:
    virtual ~Stream() = default;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual size_t write(uint8_t) = 0;
};

#else

#include <Stream.h>

#endif

namespace exec {

using Stream = ::Stream;

}  // namespace exec
