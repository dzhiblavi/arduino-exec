#pragma once

#include "exec/io/encoder/sm/AsyncEncoder.h"

#ifdef EXEC_ARDUINO

#include "exec/os//Service.h"

namespace exec {

class AsyncEncoderService : public AsyncEncoder, public Service {
 public:
    AsyncEncoderService();
    void tick() override;
};

}  // namespace exec

#endif
