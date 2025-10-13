#pragma once

#include "exec/Runnable.h"

namespace exec {

class Executor {
 public:
    virtual ~Executor() = default;
    virtual void post(Runnable* r) = 0;
};

}  // namespace exec
