#pragma once

#include "exec/executor/Executor.h"
#include "exec/os/Service.h"
#include "exec/sm/Initiator.h"

namespace exec {

[[nodiscard]] inline Initiator auto yield() {
    return [](Runnable* cb, CancellationSlot /*slot*/ = {}) {
        service<Executor>()->post(cb);
        return noop;
    };
}

[[nodiscard]] Runnable* yield(Runnable* ctx) {
    return yield()(ctx);
}

}  // namespace exec
