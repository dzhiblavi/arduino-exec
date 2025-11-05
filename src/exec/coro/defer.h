#pragma once

#include "exec/Error.h"
#include "exec/os/DeferService.h"

#include <supp/Pinned.h>
#include <time/mono.h>

#include <coroutine>

namespace exec {

// Not cancellable
inline auto defer(ttime::Duration d) {
    struct [[nodiscard]] Awaitable : Runnable, supp::Pinned {
        Awaitable(ttime::Duration d) : d{d} {}

        bool await_ready() const {
            if (d.micros() == 0) {
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

        Runnable* run() override {
            caller.resume();
            return noop;
        }

        const ttime::Duration d;
        std::coroutine_handle<> caller;
        ErrCode code_ = ErrCode::Unknown;
    };

    struct Op {
        auto operator co_await() { return Awaitable{d}; }
        const ttime::Duration d;
    };

    return Op{d};
}

}  // namespace exec
