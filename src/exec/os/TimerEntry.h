#pragma once

#include "exec/Runnable.h"

#include <supp/IntrusivePriorityQueue.h>

#include <time/time.h>

namespace exec {

struct TimerEntry : supp::IntrusivePriorityQueueNode {
    ttime::Time at;
    Runnable* task = nullptr;
};

}  // namespace exec
