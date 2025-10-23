#pragma once

#include "exec/io/stream/Stream.h"

#include "exec/sm/Initiator.h"
#include "exec/sm/Operation.h"
#include "exec/sm/yield.h"

#include "exec/Error.h"

#include <logging/log.h>

namespace exec {

class AsyncStream : Runnable {
 public:
    explicit AsyncStream(Stream* stream) : stream_{stream} {}

    // NOTE: Equivalent to the default constructor.
    AsyncStream(AsyncStream&& rhs) noexcept : AsyncStream(rhs.stream_) {}

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

        LTRACE("Stream::read init async dst=", dst_, ", left=", *left_);
        op_.initiate(ec, cb, slot);
        return yield()(this);
    }

    // Runnable
    Runnable* run() override {
        if (!op_.outstanding()) {
            LTRACE("Stream::read cancelled dst=", dst_, ", left=", *left_);
            return noop;
        }

        if (performRead()) {
            return op_.complete(ErrCode::Success);
        }

        return yield()(this);
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

    Stream* const stream_;
    char* dst_ = nullptr;
    int* left_ = 0;
    CancellableOperation<> op_;
};

}  // namespace exec
