#include "exec/sys/config.h"  // IWYU pragma: keep

#if defined(EXEC_NATIVE)

#include "exec/sys/memory/usage.h"  // IWYU pragma: keep

namespace exec {

MemoryUsage getMemoryUsage() {
    return {.free_ram = static_cast<size_t>(-1)};
}

}  // namespace exec

#endif
