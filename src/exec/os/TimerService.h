#pragma once

#include "exec/Runnable.h"
#include "exec/os/OS.h"

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

TimerService* timerService();
void setTimerService(TimerService* s);

template <int MaxTimers>
class HeapTimerService : public TimerService, public Service {
 public:
    HeapTimerService() {
        if (auto o = os()) {
            o->addService(this);
        }

        setTimerService(this);
    }

    void add(TimerEntry* t) override {
        DASSERT(!t->connected());
        heap_.push(t);
    }

    bool remove(TimerEntry* t) override {
        return heap_.erase(t);
    }

    void tick() override {
        auto now = ttime::mono::now();

        while (!heap_.empty() && now >= heap_.front()->at) {
            heap_.pop()->task->runAll();
        }
    }

 private:
    using Comp = decltype([](auto& l, auto& r) { return l.at < r.at; });
    supp::IntrusivePriorityQueue<TimerEntry, MaxTimers, Comp> heap_;
};

}  // namespace exec
