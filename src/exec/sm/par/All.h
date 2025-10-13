#pragma once

#include "exec/sm/par/ParGroup.h"

namespace exec {

template <size_t N>
using All = detail::ParGroup<N, detail::ParGroupType::All>;

}  // namespace exec
