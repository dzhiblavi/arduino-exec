#pragma once

#include "exec/Unit.h"
#include "exec/os/DeferService.h"

#include <time/mono.h>

#include <coroutine>

namespace exec {

// Not cancellable
auto defer(ttime::Duration d) {
    struct Awaitable : Runnable {
        Awaitable(ttime::Duration d) : d{d} {}

        bool await_ready() const noexcept {
            return d.micros() == 0;
        }

        // d > 0 (see await_ready)
        void await_suspend(std::coroutine_handle<> caller) {
            this->caller = caller;
            service<DeferService>()->defer(this, ttime::mono::now() + d);
        }

        Unit await_resume() const noexcept {
            return unit;
        }

        Runnable* run() override {
            caller.resume();
            return noop;
        }

        std::coroutine_handle<> caller;
        ttime::Duration d;
    };

    return Awaitable{d};
}

}  // namespace exec
