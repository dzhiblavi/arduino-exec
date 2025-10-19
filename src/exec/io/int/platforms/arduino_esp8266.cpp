#include "exec/sys/config.h" // IWYU pragma: keep

#if defined(EXEC_ARDUINO_ESP8266)

#include "exec/io/int/interrupts.h"

#include <Arduino.h>

namespace exec {

namespace {

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
    ::attachInterrupt(int_no, func, implMode(mode));
}

void attachInterruptArg(uint8_t int_no, InterruptArgFunc func, void* arg, InterruptMode mode) {
    ::attachInterruptArg(int_no, func, arg, implMode(mode));
}

void detachInterrupt(uint8_t int_no) {
    ::detachInterrupt(int_no);
}

}  // namespace exec

#endif
