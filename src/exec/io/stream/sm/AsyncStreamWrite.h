#pragma once

#include "exec/io/stream/Stream.h"

#include "exec/sm/Initiator.h"
#include "exec/sm/Operation.h"
#include "exec/sm/yield.h"

#include "exec/Error.h"

#include <logging/log.h>

#include <algorithm>

namespace exec {

class AsyncPrintWrite : Runnable, CancellableOperation<> {
 public:
    explicit AsyncPrintWrite(Print* print) : print_{print} {}

    // NOTE: Equivalent to the Stream* constructor.
    AsyncPrintWrite(AsyncPrintWrite&& rhs) noexcept : AsyncPrintWrite(rhs.print_) {}

    Initiator auto operator()(const char* buf, int* count, ErrCode* err) {
        return [this, buf, count, err](Runnable* cb, CancellationSlot slot = {}) {
            return write(buf, count, err, cb, slot);
        };
    }

 private:
    Runnable* write(const char* buf, int* count, ErrCode* ec, Runnable* cb, CancellationSlot slot) {
        buf_ = buf;
        left_ = count;

        if (performWrite()) {
            *ec = ErrCode::Success;
            return cb;
        }

        LTRACE("write init async dst=", buf_, ", left=", *left_);
        initiate(ec, cb, slot);
        return yield(this);
    }

    // Runnable
    Runnable* run() override {
        if (!outstanding()) {
            LTRACE("write cancelled dst=", buf_, ", left=", *left_);
            return noop;
        }

        if (performWrite()) {
            return complete(ErrCode::Success);
        }

        return yield(this);
    }

    bool performWrite() {
        while (*left_ > 0) {
            int available = print_->availableForWrite();
            if (available <= 0) {
                return false;
            }

            int write = std::min(available, *left_);
            write = static_cast<int>(print_->write(buf_, write));

            LINFO("write wrote ", write, " bytes");
            *left_ -= write;
            buf_ += write;  // NOLINT
        }

        return *left_ == 0;
    }

    Print* const print_;
    const char* buf_ = nullptr;
    int* left_ = 0;
};

}  // namespace exec
