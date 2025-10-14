#include "exec/sys/config.h"

#if defined(EXEC_SYSTEM_ARDUINO_ESP8266)

#include "exec/sys/int/interrupts.h"

#include <Arduino.h>

namespace exec {

void attachInterrupt(uint8_t int_no, InterruptFunc func, InterruptMode mode) {
    ::attachInterrupt(int_no, func, static_cast<int>(mode));
}

void attachInterruptArg(uint8_t int_no, InterruptArgFunc func, void* arg, InterruptMode mode) {
    ::attachInterruptArg(int_no, func, arg, static_cast<int>(mode));
}

void detachInterrupt(uint8_t int_no) {
    ::detachInterrupt(int_no);
}

}  // namespace exec

#endif
