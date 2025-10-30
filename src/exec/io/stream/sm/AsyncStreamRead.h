#pragma once

#include "exec/io/stream/Stream.h"

#include "exec/sm/Initiator.h"
#include "exec/sm/Operation.h"
#include "exec/sm/yield.h"

#include "exec/Error.h"

#include <logging/log.h>

namespace exec {

class AsyncStreamRead : Runnable, CancellableOperation<> {
 public:
    explicit AsyncStreamRead(Stream* stream) : stream_{stream} {}

    // NOTE: Equivalent to the Stream* constructor.
    AsyncStreamRead(AsyncStreamRead&& rhs) noexcept : AsyncStreamRead(rhs.stream_) {}

    Initiator auto operator()(char* dst, int* count, ErrCode* err) {
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

        LTRACE("read init async dst=", dst_, ", left=", *left_);
        initiate(ec, cb, slot);
        return yield(this);
    }

    // Runnable
    Runnable* run() override {
        if (!outstanding()) {
            LTRACE("read cancelled dst=", dst_, ", left=", *left_);
            return noop;
        }

        if (performRead()) {
            return complete(ErrCode::Success);
        }

        return yield(this);
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
};

}  // namespace exec
