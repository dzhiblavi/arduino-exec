#include "exec/os/service/TimerService.h"

namespace exec {

namespace {

TimerService* service = nullptr;

}  // namespace

TimerService* timerService() {
    return service;
}

void setTimerService(TimerService* s) {
    DASSERT(service == nullptr, "TimerService already set");
    service = s;
}

}  // namespace exec
