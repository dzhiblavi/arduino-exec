#pragma once

#include <cstdint>

namespace exec {

enum class ErrCode : uint8_t {
    Unknown = 0,
    Cancelled,
    Success,
};

}  // namespace exec
