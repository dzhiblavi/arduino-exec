#include <exec/coro/sync/Mutex.h>

#include <cstddef>

namespace exec::test {

size_t lockGuardSizeFromAnotherTranslationUnit() {
    return sizeof(LockGuard);
}

}  // namespace exec::test
