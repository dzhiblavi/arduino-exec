#pragma once

#include "exec/executor/Executor.h"

namespace exec {

Executor* executor();

void setExecutor(Executor* exec);

}  // namespace exec
