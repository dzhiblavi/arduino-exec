#pragma once

#include "exec/sys/config.h"  // IWYU pragma: keep

#include <stdint.h>

namespace exec {

using InterruptFunc = void (*)();
using InterruptArgFunc = void (*)(void*);

enum class InterruptMode : uint8_t {
    Change = 1,
    Falling = 2,
    Rising = 3,
};

void attachInterrupt(uint8_t int_no, InterruptFunc func, InterruptMode mode);
void attachInterruptArg(uint8_t int_no, InterruptArgFunc func, void* arg, InterruptMode mode);
void detachInterrupt(uint8_t int_no);

#if defined(EXEC_SYSTEM_NATIVE)
void raiseInterrupt(uint8_t int_no, InterruptMode mode);
#endif

}  // namespace exec
