#pragma once

#include "exec/executor/Executor.h"
#include "exec/sm/Initiator.h"

namespace exec {

[[nodiscard]] inline Initiator auto yield() {
    return [](Runnable* cb, CancellationSlot /*slot*/ = {}) {
        executor()->post(cb);
        return noop;
    };
}

}  // namespace exec
