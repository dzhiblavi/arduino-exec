#pragma once

#include "exec/Runnable.h"
#include "exec/os/ServiceBase.h"

#include <supp/RandomAccessPriorityQueue.h>

#include <time/mono.h>

namespace exec {

struct CronTask : supp::RandomAccessPriorityQueueNode {
    CronTask(Runnable* task, ttime::Duration interval, ttime::Time at = ttime::mono::now())
        : task{task}
        , interval{interval}
        , at{at} {}

    CronTask(ttime::Duration interval, ttime::Time at = ttime::mono::now())
        : CronTask(noop, interval, at) {}

    Runnable* task;
    ttime::Duration interval;
    ttime::Time at = ttime::mono::now();
};

// Starts tasks as close to their interval as possible.
// If:
//   - task's execution time is larger than the interval;
//   - or task moves itself to another CronService,
// the behaviour is undefined.
//
// If task is asynchronous, it should remove itself from cron before async part and
// return itself back when done.
class CronService {
 public:
    virtual ~CronService() = default;
    virtual void add(CronTask* task) = 0;
    virtual bool remove(CronTask* task) = 0;
};

template <int MaxTasks>
class HeapCronService : public CronService,
                        public ServiceBase<CronService, HeapCronService<MaxTasks>> {
 public:
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
            front->at = now + front->interval;
            front->task->runAll();  // may call remove()

            if (front->connected()) {
                heap_.fix(front);
            }
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
