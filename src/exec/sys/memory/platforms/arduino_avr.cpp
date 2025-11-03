#include "exec/sys/config.h"  // IWYU pragma: keep

#if defined(EXEC_ARDUINO_AVR)

#include "exec/sys/memory/usage.h"  // IWYU pragma: keep

extern int __heap_start, *__brkval;

namespace exec {

MemoryUsage getMemoryUsage() {
    int v;
    int free_ram = (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);

    return {
        .free_ram = static_cast<size_t>(free_ram),
    };
}

}  // namespace exec

#endif
