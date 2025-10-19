#pragma once

#include "exec/io/int/config.h"  // IWYU pragma: keep

#include <cstdint>

namespace exec {

using InterruptFunc = void (*)();
using InterruptArgFunc = void (*)(void*);

enum class InterruptMode : uint8_t {
    Change,
    Falling,
    Rising,
};

void attachInterrupt(uint8_t int_no, InterruptFunc func, InterruptMode mode);
void attachInterruptArg(uint8_t int_no, InterruptArgFunc func, void* arg, InterruptMode mode);
void detachInterrupt(uint8_t int_no);

#if defined(EXEC_INT_MANUAL)
void raiseInterrupt(uint8_t int_no, InterruptMode mode);
#endif

}  // namespace exec
