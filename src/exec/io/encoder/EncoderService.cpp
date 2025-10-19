#include "exec/io/encoder/EncoderService.h"

#ifdef EXEC_ARDUINO

#include "exec/os/OS.h"

namespace exec {

EncoderService::EncoderService() {
    if (auto o = os()) {
        o->addService(this);
    }
}

void EncoderService::tick() {
    Encoder::tick();
}

}  // namespace exec

#endif
