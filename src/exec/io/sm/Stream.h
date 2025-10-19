#pragma once

#include "exec/Error.h"
#include "exec/executor/Executor.h"
#include "exec/sm/Initiator.h"
#include "exec/sm/Operation.h"

#include <log.h>

#if !__has_include(<Stream.h>)

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

using Source = ::Stream;

class Stream : Runnable {
 public:
    explicit Stream(Source* stream) : stream_{stream} {}

    // NOTE: Equivalent to the default constructor.
    Stream(Stream&& rhs) noexcept : Stream(rhs.stream_) {}

    Initiator auto read(char* dst, int* count, ErrCode* err) {
        return [this, dst, count, err](Runnable* cb, CancellationSlot slot = {}) {
            return read(dst, count, err, cb, slot);
        };
    }

 private:
    Runnable* read(char* dst, int* count, ErrCode* ec, Runnable* cb, CancellationSlot slot) {
        dst_ = dst;
        left_ = count;

        if (performRead()) {
            *ec = ErrCode::Success;
            return cb;
        }

        LTRACE("Stream::read init async dst=%X left=%d", dst_, *left_);
        op_.initiate(ec, cb, slot);
        executor()->post(this);
        return noop;
    }

    // Runnable
    Runnable* run() override {
        if (!op_.outstanding()) {
            LTRACE("Stream::read cancelled dst=%X left=%d", dst_, *left_);
            return noop;
        }

        if (performRead()) {
            return op_.complete(ErrCode::Success);
        }

        executor()->post(this);
        return noop;
    }

    bool performRead() {
        while (*left_ > 0) {
            int b = stream_->read();
            if (b == -1) {
                break;
            }

            --*left_;
            *(dst_++) = static_cast<char>(b);  // NOLINT
        }

        return *left_ == 0;
    }

    Source* const stream_;
    char* dst_ = nullptr;
    int* left_ = 0;
    CancellableOperation<> op_;
};

}  // namespace exec
