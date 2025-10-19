#pragma once

#include "exec/io/encoder/Encoder.h"  // IWYU pragma: keep

#ifdef EXEC_ARDUINO

#include "exec/os/Service.h"

namespace exec {

class EncoderService : public Encoder, public Service {
 public:
    EncoderService();
    void tick() override;
};

}  // namespace exec

#endif
