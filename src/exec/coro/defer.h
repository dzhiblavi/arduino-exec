#pragma once

#include "exec/Error.h"
#include "exec/coro/traits.h"
#include "exec/os/DeferService.h"

#include <supp/Pinned.h>
#include <time/mono.h>

#include <coroutine>

namespace exec {

// Not cancellable
inline Awaitable auto defer(ttime::Duration d) {
    struct [[nodiscard]] Awaiter : Runnable, supp::Pinned {
        Awaiter(ttime::Duration d) : d{d} {}

        bool await_ready() {
            if (d.micros() == 0) {
                code_ = ErrCode::Success;
                return true;
            }

            return false;
        }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) {
            if (!service<DeferService>()->defer(this, ttime::mono::now() + d)) {
                return caller;
            }

            this->caller = caller;
            return std::noop_coroutine();
        }

        ErrCode await_resume() const {
            DASSERT(code_ != ErrCode::Unknown);
            return code_;
        }

        void run() override { caller.resume(); }

        const ttime::Duration d;
        std::coroutine_handle<> caller;
        ErrCode code_ = ErrCode::Unknown;
    };

    struct Awaitable {
        auto operator co_await() { return Awaiter{d}; }
        const ttime::Duration d;
    };

    return Awaitable{d};
}

}  // namespace exec
