#pragma once

#include "exec/Error.h"
#include "exec/os/service/TimerService.h"
#include "exec/sm/Initiator.h"
#include "exec/sm/Operation.h"

#include <supp/Pinned.h>

#include <log.h>
#include <time/mono.h>

namespace exec {

class Timer : public Runnable, private CancellationHandler {
 public:
    Timer() = default;

    // NOTE: Equivalent to the default constructor.
    Timer(Timer&&) noexcept : Timer() {}

    [[nodiscard]] Initiator auto wait(ttime::Duration dur, ErrCode* code) {
        return [this, dur, code](Runnable* cb, CancellationSlot slot = {}) -> Runnable* {
            doWait(dur, code, cb, slot);
            return noop;
        };
    }

 private:
    void doWait(ttime::Duration dur, ErrCode* ec, Runnable* cb, CancellationSlot slot) {
        op_.initiate(ec, cb, slot, this);
        entry_.at = ttime::mono::now() + dur;
        entry_.task = this;
        timerService()->add(&entry_);
    }

    // Runnable, called by the OS when timer goes off
    Runnable* run() override {
        return op_.complete(ErrCode::Success);
    }

    // CancellationHandler, called by the cancellation signal
    Runnable* cancel() override {
        op_.detachCancellation();

        if (!timerService()->remove(&entry_)) {
            // Timer has already went off or has been cancelled, nothing to do here
            return noop;
        }

        // Timer has been removed from OS's timers queue,
        // so run() method will never be called
        return op_.complete(ErrCode::Cancelled);
    }

    TimerEntry entry_;
    Operation op_;
};

}  // namespace exec
