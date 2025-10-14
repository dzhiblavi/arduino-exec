#pragma once

#include "exec/Runnable.h"
#include "exec/executor/Executor.h"
#include "exec/os/Service.h"

#include <supp/IntrusivePriorityQueue.h>

#include <time/mono.h>

namespace exec {

struct TimerEntry : supp::IntrusivePriorityQueueNode {
    ttime::Time at;
    Runnable* task = nullptr;
};

class TimerService {
 public:
    virtual ~TimerService() = default;

    virtual void add(TimerEntry* t) = 0;
    virtual bool remove(TimerEntry* t) = 0;
};

template <int MaxTimers>
class TimerServiceImpl : public TimerService, public Service {
 public:
    TimerServiceImpl() = default;

    void add(TimerEntry* t) override {
        DASSERT(!t->connected());
        timers_.push(t);
    }

    bool remove(TimerEntry* t) override {
        return timers_.erase(t);
    }

    void tick() override {
        auto now = ttime::mono::now();

        while (!timers_.empty() && now >= timers_.front()->at) {
            executor()->post(timers_.pop()->task);
        }
    }

 private:
    using Comp = decltype([](auto& l, auto& r) { return l.at < r.at; });
    supp::IntrusivePriorityQueue<TimerEntry, MaxTimers, Comp> timers_;
};

TimerService* timerService();
void setTimerService(TimerService* s);

}  // namespace exec
