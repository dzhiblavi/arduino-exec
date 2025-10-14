#include "exec/os/service/TimerService.h"

namespace exec {

namespace {

TimerService* service = nullptr;

}  // namespace

TimerService* timerService() {
    return service;
}

void setTimerService(TimerService* s) {
    service = s;
}

}  // namespace exec
