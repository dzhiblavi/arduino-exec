#include "exec/os/OS.h"

namespace exec {

namespace {

OS* current_ = nullptr;

}  // namespace

OS* os() {
    return current_;
}

void setOS(OS* os) {
    DASSERT(current_ == nullptr, "OS already set");
    current_ = os;
}

}  // namespace exec
