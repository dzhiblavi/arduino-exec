#include "exec/os/global.h"

#include <supp/verify.h>

namespace exec {

namespace {

OS* os_ = nullptr;

}  // namespace

OS* os() {
    DASSERT(os_ != nullptr);
    return os_;
}

void setOS(OS* os) {
    os_ = os;
}

}  // namespace exec
