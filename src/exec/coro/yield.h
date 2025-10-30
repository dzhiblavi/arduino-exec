#pragma once

#include "exec/Unit.h"
#include "exec/executor/Executor.h"
#include "exec/os/Service.h"

#include <coroutine>

namespace exec {

// Not cancellable
auto yield() {
    struct Awaitable : Runnable {
        constexpr bool await_ready() const noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<> caller) noexcept {
            this->caller = caller;
            service<Executor>()->post(this);
        }

        constexpr Unit await_resume() noexcept {
            return unit;
        }

        Runnable* run() override {
            caller.resume();
            return noop;
        }

        std::coroutine_handle<> caller;
    };

    return Awaitable{};
}

}  // namespace exec
