#include "exec/coro/alloc.h"

namespace exec::alloc {

__attribute__((weak)) void* allocate(size_t size, const std::nothrow_t& tag) noexcept {
    return ::operator new(size, tag);
}

__attribute__((weak)) void deallocate(void* ptr, size_t) noexcept {
    return ::operator delete(ptr);
}

}  // namespace exec::alloc
