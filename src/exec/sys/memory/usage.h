#pragma once

#include <cstddef>

namespace exec {

struct MemoryUsage {
    size_t free_ram;
};

MemoryUsage getMemoryUsage();

}  // namespace exec
