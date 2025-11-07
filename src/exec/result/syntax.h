#pragma once

#include "exec/result/Result.h"

#include <utility>

template <typename T, typename C>
auto operator|(exec::Result<T> r, C c) {
    return c.pipe(std::move(r));
}
