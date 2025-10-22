#include "exec/os/OS.h"

#include <algorithm>

namespace exec {

OS::OS() {
    setService<OS>(this);
}

void OS::tick() {
    services_.iterate([](Service& s) { s.tick(); });
}

void OS::addService(Service* s) {
    services_.pushBack(s);
}

ttime::Time OS::wakeAt() const {
    ttime::Time res = ttime::Time::max();
    services_.iterate([&res](const Service& s) { res = std::min(res, s.wakeAt()); });
    return res;
}

}  // namespace exec
