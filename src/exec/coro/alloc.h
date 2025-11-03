#pragma once

#include <cstddef>
#include <new>

namespace exec::alloc {

void* allocate(size_t, const std::nothrow_t&) noexcept;
void deallocate(void*, size_t) noexcept;

}  // namespace exec::alloc
