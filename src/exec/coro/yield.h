#pragma once

#include "exec/Unit.h"
#include "exec/coro/traits.h"
#include "exec/executor/Executor.h"
#include "exec/os/Service.h"

#include <supp/Pinned.h>

#include <coroutine>

namespace exec {

// Not cancellable
inline Awaitable auto yield() {
    struct [[nodiscard]] Awaiter : Runnable, supp::Pinned {
        Awaiter() = default;

        constexpr bool await_ready() const { return false; }

        void await_suspend(std::coroutine_handle<> caller) {
            this->caller = caller;
            service<Executor>()->post(this);
        }

        constexpr Unit await_resume() { return unit; }

        void run() override { caller.resume(); }

        std::coroutine_handle<> caller;
    };

    struct Awaitable {
        auto operator co_await() const { return Awaiter{}; }
    };

    return Awaitable{};
}

}  // namespace exec
