#pragma once

#include "exec/sm/par/ParGroup.h"

namespace exec {

template <size_t N>
using Any = detail::ParGroup<N, detail::ParGroupType::Any>;

}  // namespace exec
