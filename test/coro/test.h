#pragma once

#include "exec/coro/alloc.h"

#include <unity.h>

namespace exec {

struct t_coro {
    t_coro() = default;
    ~t_coro() { TEST_ASSERT_EQUAL(0, alloc::allocatedCount()); }
};

}  // namespace exec
