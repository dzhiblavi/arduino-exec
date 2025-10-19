#pragma once

#include <cstdint>

namespace exec {

enum class ErrCode : uint8_t {
    Success = 0,
    Cancelled,
    Unknown,
};

}  // namespace exec
