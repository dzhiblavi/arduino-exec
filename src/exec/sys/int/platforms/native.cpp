#include "exec/sys/config.h"  // IWYU pragma: keep

#if defined(EXEC_SYSTEM_NATIVE)

#include "exec/sys/int/interrupts.h"

#include <supp/verify.h>

namespace exec {

namespace {

static constexpr int NumInterrupts = 2;
using UserType = void*;

InterruptMode modes_[NumInterrupts] = {};
UserType user_args_[NumInterrupts] = {nullptr};
InterruptFunc user_funcs0_[NumInterrupts] = {nullptr};
InterruptArgFunc user_funcs1_[NumInterrupts] = {nullptr};

void int0() {
    user_funcs1_[0](user_args_[0]);
}

void int1() {
    user_funcs1_[1](user_args_[1]);
}

}  // namespace

void attachInterrupt(uint8_t int_no, InterruptFunc func, InterruptMode mode) {
    DASSERT(int_no < NumInterrupts);
    modes_[int_no] = mode;
    user_funcs0_[int_no] = func;
}

void attachInterruptArg(uint8_t int_no, InterruptArgFunc func, void* arg, InterruptMode mode) {
    DASSERT(int_no < NumInterrupts);
    user_args_[int_no] = arg;
    user_funcs1_[int_no] = func;
    attachInterrupt(int_no, int_no == 0 ? int0 : int1, mode);
}

void detachInterrupt(uint8_t int_no) {
    user_funcs0_[int_no] = nullptr;
}

void raiseInterrupt(uint8_t int_no, InterruptMode mode) {
    if (mode != modes_[int_no]) {
        return;
    }

    if (user_funcs0_[int_no] == nullptr) {
        return;
    }

    user_funcs0_[int_no]();
}

}  // namespace exec

#endif
