#pragma once

#include "exec/Runnable.h"
#include "exec/cancel.h"

namespace exec {

// Initiator should never call the callback directly
template <typename T>
concept Initiator = requires(T&& init, Runnable* cb, CancellationSlot slot) {
    { init(cb) } -> std::same_as<Runnable*>;
    { init(cb, slot) } -> std::same_as<Runnable*>;
};

}  // namespace exec
