#include "exec/coro/alloc.h"

namespace exec::alloc {

namespace {

size_t allocated_count_ = 0;

}  // namespace

size_t allocatedCount() noexcept {
    return allocated_count_;
}

__attribute__((weak)) void* allocate(size_t size, const std::nothrow_t& tag) noexcept {
    ++allocated_count_;
    return ::operator new(size, tag);
}

__attribute__((weak)) void deallocate(void* ptr, size_t) noexcept {
    --allocated_count_;
    return ::operator delete(ptr);
}

}  // namespace exec::alloc
