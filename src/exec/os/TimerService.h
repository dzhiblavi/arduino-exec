#pragma once

#include "exec/Runnable.h"
#include "exec/os/ServiceBase.h"

#include <supp/RandomAccessPriorityQueue.h>

#include <time/mono.h>

namespace exec {

struct TimerEntry : Runnable, supp::RandomAccessPriorityQueueNode {
    ttime::Time at;
};

class TimerService {
 public:
    virtual ~TimerService() = default;

    [[nodiscard]] virtual bool add(TimerEntry* t) = 0;
    [[nodiscard]] virtual bool remove(TimerEntry* t) = 0;
};

template <int MaxTimers>
class HeapTimerService : public TimerService,
                         public ServiceBase<TimerService, HeapTimerService<MaxTimers>> {
 public:
    bool add(TimerEntry* t) override {
        DASSERT(!t->connected());
        return heap_.push(t);
    }

    bool remove(TimerEntry* t) override { return heap_.erase(t); }

    void tick() override {
        auto now = ttime::mono::now();

        while (!heap_.empty() && now >= heap_.front()->at) {
            heap_.pop()->run();
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
