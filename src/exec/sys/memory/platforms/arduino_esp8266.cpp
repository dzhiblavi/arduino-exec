#include "exec/sys/config.h"  // IWYU pragma: keep

#if defined(EXEC_ARDUINO_ESP8266)

#include "exec/sys/memory/usage.h"  // IWYU pragma: keep

#include <ESP.h>

namespace exec {

MemoryUsage getMemoryUsage() {
    return {
        .free_ram = ESP.getFreeHeap(),
    };
}

}  // namespace exec

#endif
