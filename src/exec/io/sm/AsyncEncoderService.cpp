#include "exec/io/sm/AsyncEncoderService.h"

#ifdef EXEC_ARDUINO

#include "exec/os/OS.h"

namespace exec {

AsyncEncoderService::AsyncEncoderService() {
    if (auto o = os()) {
        o->addService(this);
    }
}

void AsyncEncoderService::tick() {
    AsyncEncoder::tick();
}

}  // namespace exec

#endif
