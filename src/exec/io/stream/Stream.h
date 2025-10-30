#pragma once

#if !__has_include(<Stream.h>)

#include <cstdint>
#include <cstddef>

class Stream {
 public:
    virtual ~Stream() = default;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
};

class Print {
 public:
    virtual ~Print() = default;
    virtual int availableForWrite() = 0;
    virtual size_t write(const char*, size_t) = 0;
};

#else

#include <Stream.h>

#endif

namespace exec {

using Stream = ::Stream;
using Print = ::Print;

}  // namespace exec
