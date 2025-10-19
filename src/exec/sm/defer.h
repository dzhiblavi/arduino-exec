#pragma once

#include "exec/os//DeferService.h"
#include "exec/sm/Initiator.h"

#include <time/mono.h>

namespace exec {

[[nodiscard]] inline Initiator auto defer(ttime::Duration d) {
    return [d](Runnable* cb, CancellationSlot /*slot*/ = {}) {
        deferService()->defer(cb, ttime::mono::now() + d);
        return noop;
    };
}

}  // namespace exec
