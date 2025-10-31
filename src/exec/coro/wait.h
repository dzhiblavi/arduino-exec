#pragma once

#include "exec/Error.h"
#include "exec/cancel.h"
#include "exec/os/TimerService.h"

#include <logging/log.h>
#include <supp/Pinned.h>
#include <time/mono.h>

#include <coroutine>

namespace exec {

// Cancellable delay
auto wait(ttime::Duration d) {
    struct [[nodiscard]] Awaitable : Runnable, CancellationHandler, supp::Pinned {
        Awaitable(ttime::Duration d) noexcept : d{d} {}

        bool await_ready() noexcept {
            if (d.micros() > 0) {
                return false;
            }

            slot_.clearIfConnected();
            return true;
        }

        void await_suspend(std::coroutine_handle<> awaiter) noexcept {
            this->awaiter = awaiter;
            entry_.at = ttime::mono::now() + d;
            entry_.task = this;
            service<TimerService>()->add(&entry_);
        }

        ErrCode await_resume() const noexcept {
            DASSERT(code_ != ErrCode::Unknown);
            return code_;
        }

        // called when timer went off
        Runnable* run() noexcept override {
            DASSERT(code_ == ErrCode::Unknown);
            slot_.clearIfConnected();
            code_ = ErrCode::Success;
            awaiter.resume();
            return noop;
        }

        // called when cancellation is signalled
        Runnable* cancel() noexcept override {
            slot_.clearIfConnected();

            if (!service<TimerService>()->remove(&entry_)) {
                // Timer has already went off or has been cancelled, nothing to do here
                return noop;
            }

            code_ = ErrCode::Cancelled;
            awaiter.resume();
            return noop;
        }

        // CancellableAwaitable
        Awaitable& setCancellationSlot(CancellationSlot slot) noexcept {
            slot_ = slot;
            slot_.installIfConnected(this);
            return *this;
        }

        ErrCode code_ = ErrCode::Unknown;
        CancellationSlot slot_;
        std::coroutine_handle<> awaiter;
        TimerEntry entry_;
        const ttime::Duration d;
    };

    return Awaitable{d};
}

}  // namespace exec
