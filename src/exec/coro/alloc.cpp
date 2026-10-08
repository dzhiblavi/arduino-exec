#include "exec/coro/alloc.h"

#include <cstdlib>

namespace exec::alloc {

namespace {

size_t allocated_count_ = 0;

}  // namespace

size_t allocatedCount() noexcept {
    return allocated_count_;
}

void* allocate(size_t size) {
    auto* ptr = allocate(size, std::nothrow);

    if (ptr == nullptr) {
#ifdef __cpp_exceptions
        throw std::bad_alloc{};
#else
        abort();
#endif
    }

    return ptr;
}

__attribute__((weak)) void* allocate(size_t size, const std::nothrow_t& tag) noexcept {
    auto* ptr = ::operator new(size, tag);

    if (ptr != nullptr) {
        ++allocated_count_;
    }

    return ptr;
}

__attribute__((weak)) void deallocate(void* ptr, size_t) noexcept {
    --allocated_count_;
    return ::operator delete(ptr);
}

}  // namespace exec::alloc
