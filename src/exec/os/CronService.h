#pragma once

#include "exec/Runnable.h"
#include "exec/os/OS.h"

#include <supp/RandomAccessPriorityQueue.h>

#include <time/mono.h>

namespace exec {

struct CronTask : supp::RandomAccessPriorityQueueNode {
    CronTask(Runnable* task, ttime::Duration interval, ttime::Time at = ttime::mono::now())
        : task{task}
        , interval{interval}
        , at{at} {}

    Runnable* task;
    ttime::Duration interval;
    ttime::Time at = ttime::mono::now();
};

class CronService {
 public:
    virtual ~CronService() = default;
    virtual void add(CronTask* task) = 0;
    virtual bool remove(CronTask* task) = 0;
};

template <int MaxTasks>
class HeapCronService : public CronService, public Service {
 public:
    HeapCronService() {
        if (auto o = os()) {
            o->addService(this);
        }

        setService<CronService>(this);
    }

    void add(CronTask* task) override {
        heap_.push(task);
    }

    bool remove(CronTask* task) override {
        return heap_.erase(task);
    }

    // Service
    void tick() override {
        auto now = ttime::mono::now();

        while (!heap_.empty() && now >= heap_.front()->at) {
            auto* front = heap_.front();
            front->task->runAll();
            front->at = now + front->interval;
            heap_.fix(front);
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
    supp::RandomAccessPriorityQueue<CronTask, MaxTasks, Comp> heap_;
};

}  // namespace exec
