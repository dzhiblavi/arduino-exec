#include "exec/io/encoder/sm/AsyncEncoderService.h"

#ifdef EXEC_ARDUINO

#include "exec/os/OS.h"

namespace exec {

AsyncEncoderService::AsyncEncoderService() {
    if (auto os = tryService<OS>()) {
        os->addService(this);
    }
}

void AsyncEncoderService::tick() {
    AsyncEncoder::tick();
}

}  // namespace exec

#endif
