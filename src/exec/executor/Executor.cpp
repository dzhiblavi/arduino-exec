#include "exec/executor/Executor.h"

namespace exec {

namespace {

Executor* executor_ = nullptr;

}  // namespace

Executor* executor() {
    DASSERT(executor_ != nullptr);
    return executor_;
}

void setExecutor(Executor* exec) {
    executor_ = exec;
}

}  // namespace exec
