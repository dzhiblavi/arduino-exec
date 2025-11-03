#pragma once

#include "exec/Unit.h"
#include "exec/os/DeferService.h"

#include <supp/Pinned.h>
#include <time/mono.h>

#include <coroutine>

namespace exec {

// Not cancellable
auto defer(ttime::Duration d) {
    struct [[nodiscard]] Awaitable : Runnable, supp::Pinned {
        Awaitable(ttime::Duration d) : d{d} {}

        bool await_ready() const { return d.micros() == 0; }

        void await_suspend(std::coroutine_handle<> caller) {
            this->caller = caller;
            service<DeferService>()->defer(this, ttime::mono::now() + d);
        }

        Unit await_resume() const { return unit; }

        Runnable* run() override {
            caller.resume();
            return noop;
        }

        std::coroutine_handle<> caller;
        const ttime::Duration d;
    };

    return Awaitable{d};
}

}  // namespace exec
