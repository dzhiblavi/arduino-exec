#pragma once

#include "exec/os/global.h"
#include "exec/sm/Initiator.h"

#include <time/mono.h>

namespace exec {

[[nodiscard]] inline Initiator auto defer(ttime::Duration d) {
    return [d](Runnable* cb, CancellationSlot /*slot*/ = {}) {
        os()->defer(cb, ttime::mono::now() + d);
        return noop;
    };
}

}  // namespace exec
