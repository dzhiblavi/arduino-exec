#pragma once

#include "exec/io/Encoder.h"

#ifdef EXEC_ARDUINO

#include "exec/os//Service.h"

namespace exec {

class EncoderService : public Encoder, public Service {
 public:
    EncoderService();
    void tick() override;
};

}  // namespace exec

#endif
