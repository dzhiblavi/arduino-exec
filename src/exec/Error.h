#pragma once

#include <stdint.h>

namespace exec {

enum class ErrCode : uint8_t {
    Success = 0,
    Cancelled,
    Unknown,
};

}  // namespace exec
