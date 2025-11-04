#pragma once

#include <cstddef>
#include <new>

namespace exec::alloc {

size_t allocatedCount() noexcept;

void* allocate(size_t, const std::nothrow_t&) noexcept;
void deallocate(void*, size_t) noexcept;

}  // namespace exec::alloc
