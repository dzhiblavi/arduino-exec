#include "exec/io/int/config.h"  // IWYU pragma: keep

#if defined(EXEC_INT_ARDUINO_AVR)

#include "exec/io/int/interrupts.h"

#include <supp/verify.h>

#include <Arduino.h>

namespace exec {

namespace {

static constexpr int NumInterrupts = 2;

using UserType = void*;
UserType user_args_[NumInterrupts] = {nullptr};
InterruptArgFunc user_funcs_[NumInterrupts] = {nullptr};

void int0() {
    user_funcs_[0](user_args_[0]);
}

void int1() {
    user_funcs_[1](user_args_[1]);
}

int implMode(InterruptMode mode) {
    switch (mode) {
        case InterruptMode::Change:
            return CHANGE;
        case InterruptMode::Rising:
            return RISING;
        case InterruptMode::Falling:
            return FALLING;
    }

    __builtin_unreachable();
}

}  // namespace

void attachInterrupt(uint8_t int_no, InterruptFunc func, InterruptMode mode) {
    DASSERT(int_no < NumInterrupts);
    ::attachInterrupt(int_no, func, implMode(mode));
}

void attachInterruptArg(uint8_t int_no, InterruptArgFunc func, void* arg, InterruptMode mode) {
    DASSERT(int_no < NumInterrupts);
    user_args_[int_no] = arg;
    user_funcs_[int_no] = func;
    attachInterrupt(int_no, int_no == 0 ? int0 : int1, mode);
}

void detachInterrupt(uint8_t int_no) {
    ::detachInterrupt(int_no);
}

}  // namespace exec

#endif
