#pragma once

#include "exec/Runnable.h"
#include "exec/os/OS.h"

#include <supp/RandomAccessPriorityQueue.h>

#include <time/mono.h>

namespace exec {

struct TimerEntry : supp::RandomAccessPriorityQueueNode {
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
class HeapTimerService : public TimerService, public Service {
 public:
    HeapTimerService() {
        if (auto o = os()) {
            o->addService(this);
        }

        setService<TimerService>(this);
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

    ttime::Time wakeAt() const override {
        if (heap_.empty()) {
            return ttime::Time::max();
        }

        return heap_.front()->at;
    }

 private:
    using Comp = decltype([](auto& l, auto& r) { return l.at < r.at; });
    supp::RandomAccessPriorityQueue<TimerEntry, MaxTimers, Comp> heap_;
};

}  // namespace exec
