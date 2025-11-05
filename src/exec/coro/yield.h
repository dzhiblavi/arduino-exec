#pragma once

#include "exec/Unit.h"
#include "exec/executor/Executor.h"
#include "exec/os/Service.h"

#include <supp/Pinned.h>

#include <coroutine>

namespace exec {

// Not cancellable
inline auto yield() {
    struct [[nodiscard]] Awaitable : Runnable, supp::Pinned {
        Awaitable() = default;

        constexpr bool await_ready() const { return false; }

        void await_suspend(std::coroutine_handle<> caller) {
            this->caller = caller;
            service<Executor>()->post(this);
        }

        constexpr Unit await_resume() { return unit; }

        void run() override { caller.resume(); }

        std::coroutine_handle<> caller;
    };

    struct Op {
        auto operator co_await() const { return Awaitable{}; }
    };

    return Op{};
}

}  // namespace exec
