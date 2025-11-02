#pragma once

#if __has_include(<Stream.h>)

#include <Stream.h>

#else

#include <cstddef>
#include <cstdint>

#endif

namespace exec {

#if __has_include(<Stream.h>)

using Stream = ::Stream;
using Print = ::Print;

#else

class Print {
 public:
    virtual ~Print() = default;
    virtual int availableForWrite() = 0;
    virtual size_t write(const char*, size_t) = 0;
};

class Stream : public Print {
 public:
    virtual ~Stream() = default;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
};

#endif

}  // namespace exec
