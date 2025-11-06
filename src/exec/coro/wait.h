#pragma once

#include "exec/Error.h"
#include "exec/coro/cancel.h"
#include "exec/coro/traits.h"
#include "exec/os/TimerService.h"

#include <supp/NonCopyable.h>
#include <time/mono.h>

#include <coroutine>

namespace exec {

// Cancellable delay
inline CancellableAwaitable auto wait(ttime::Duration d) {
    struct [[nodiscard]] Awaiter : TimerEntry, CancellationHandler {
        Awaiter(ttime::Duration d, CancellationSlot slot) : d{d}, slot_{slot} {}

        bool await_ready() {
            if (d.micros() == 0) {
                code_ = ErrCode::Success;
                return true;
            }

            return false;
        }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiter) {
            at = ttime::mono::now() + d;

            if (!service<TimerService>()->add(this)) {
                code_ = ErrCode::Exhausted;
                return awaiter;
            }

            slot_.installIfConnected(this);
            awaiter_ = awaiter;
            return std::noop_coroutine();
        }

        ErrCode await_resume() const {
            DASSERT(code_ != ErrCode::Unknown);
            return code_;
        }

        // called when timer went off
        void run() override {
            DASSERT(code_ == ErrCode::Unknown);
            slot_.clearIfConnected();
            code_ = ErrCode::Success;
            awaiter_.resume();
        }

        // called when cancellation is signalled
        std::coroutine_handle<> cancel() override {
            if (!service<TimerService>()->remove(this)) {
                // Timer has already went off or has been cancelled, nothing to do here
                return std::noop_coroutine();
            }

            code_ = ErrCode::Cancelled;
            return awaiter_;
        }

        const ttime::Duration d;
        CancellationSlot slot_;
        ErrCode code_ = ErrCode::Unknown;
        std::coroutine_handle<> awaiter_;
    };

    struct Awaitable {
        // CancellableAwaitable
        Awaitable& setCancellationSlot(CancellationSlot slot) {
            this->slot = slot;
            return *this;
        }

        auto operator co_await() { return Awaiter{d, slot}; }

        const ttime::Duration d;
        CancellationSlot slot{};
    };

    return Awaitable{d};
}

}  // namespace exec
