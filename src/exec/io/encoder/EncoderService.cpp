#include "exec/io/encoder/EncoderService.h"

#ifdef EXEC_ARDUINO

#include "exec/os/OS.h"

namespace exec {

EncoderService::EncoderService() {
    if (auto os = tryService<OS>()) {
        os->addService(this);
    }
}

void EncoderService::tick() {
    Encoder::tick();
}

}  // namespace exec

#endif
