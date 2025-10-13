#pragma once

#include "exec/Runnable.h"
#include "exec/os/TimerEntry.h"

#include <time/time.h>

namespace exec {

class OS {
 public:
    virtual ~OS() = default;
    virtual void defer(Runnable* r, ttime::Time at) = 0;
    virtual void add(TimerEntry* t) = 0;
    virtual bool remove(TimerEntry* t) = 0;
};

}  // namespace exec
