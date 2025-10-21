#include "exec/os/Service.h"

namespace exec {

ttime::Time Service::wakeAt() const {
    return ttime::Time::max();
}

}  // namespace exec
