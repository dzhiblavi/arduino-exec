#pragma once

#include "exec/Error.h"
#include "exec/cancel.h"
#include "exec/os/TimerService.h"

#include <supp/NonCopyable.h>
#include <time/mono.h>

#include <coroutine>

namespace exec {

// Cancellable delay
auto wait(ttime::Duration d) {
    struct [[nodiscard]] Awaitable : Runnable, CancellationHandler, supp::NonCopyable {
        Awaitable(ttime::Duration d, CancellationSlot slot) : d{d}, slot_{slot} {}

        bool await_ready() {
            return d.micros() == 0;
        }

        void await_suspend(std::coroutine_handle<> awaiter) {
            slot_.installIfConnected(this);
            awaiter_ = awaiter;
            entry_.at = ttime::mono::now() + d;
            entry_.task = this;
            service<TimerService>()->add(&entry_);
        }

        ErrCode await_resume() const {
            DASSERT(code_ != ErrCode::Unknown);
            return code_;
        }

        // called when timer went off
        Runnable* run() override {
            DASSERT(code_ == ErrCode::Unknown);
            slot_.clearIfConnected();
            code_ = ErrCode::Success;
            awaiter_.resume();
            return noop;
        }

        // called when cancellation is signalled
        Runnable* cancel() override {
            slot_.clearIfConnected();

            if (!service<TimerService>()->remove(&entry_)) {
                // Timer has already went off or has been cancelled, nothing to do here
                return noop;
            }

            code_ = ErrCode::Cancelled;
            awaiter_.resume();
            return noop;
        }

        const ttime::Duration d;
        CancellationSlot slot_;
        ErrCode code_ = ErrCode::Unknown;
        std::coroutine_handle<> awaiter_;
        TimerEntry entry_;
    };

    struct Op {
        // CancellableAwaitable
        Op& setCancellationSlot(CancellationSlot slot) {
            this->slot = slot;
            return *this;
        }

        auto operator co_await() { return Awaitable{d, slot}; }

        const ttime::Duration d;
        CancellationSlot slot{};
    };

    return Op{d};
}

}  // namespace exec
