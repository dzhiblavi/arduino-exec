#include "exec/os/OS.h"

namespace exec {

namespace {

OS* current_ = nullptr;

}  // namespace

OS::OS() {
    setOS(this);
}

void OS::tick() {
    services_.iterate([](Service& s) { s.tick(); });
}

void OS::addService(Service* s) {
    services_.pushBack(s);
}

OS* os() {
    return current_;
}

void setOS(OS* os) {
    DASSERT(current_ == nullptr, "OS already set");
    current_ = os;
}

}  // namespace exec
