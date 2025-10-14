#include "exec/os/service/DeferService.h"

namespace exec {

namespace {

DeferService* service = nullptr;

}  // namespace

DeferService* deferService() {
    return service;
}

void setDeferService(DeferService* s) {
    service = s;
}

}  // namespace exec
