#pragma once

#include "exec/Runnable.h"
#include "exec/cancel.h"

#include <stdlike/utility.h>

namespace exec {

// Initiator should never call the callback directly
template <typename T>
concept Initiator = requires(T&& init, Runnable* cb, CancellationSlot slot) {
    { init(cb) } -> stdlike::same_as<Runnable*>;
    { init(cb, slot) } -> stdlike::same_as<Runnable*>;
};

}  // namespace exec
