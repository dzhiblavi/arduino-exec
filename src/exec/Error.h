#pragma once

#include <cstdint>

namespace exec {

enum class [[nodiscard]] ErrCode : uint8_t {
    Unknown = 0,
    OutOfMemory,
    Cancelled,
    Exhausted,
    Success,
};

}  // namespace exec
