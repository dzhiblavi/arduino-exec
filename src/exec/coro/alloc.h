#pragma once

#include <cstddef>
#include <new>

namespace exec::alloc {

size_t allocatedCount() noexcept;

void* allocate(size_t);
void* allocate(size_t, const std::nothrow_t&) noexcept;
void deallocate(void*, size_t) noexcept;

struct PromiseAllocator {
    static void* operator new(size_t size) { return allocate(size); }
    static void operator delete(void* ptr, size_t size) noexcept { deallocate(ptr, size); }
};

}  // namespace exec::alloc
