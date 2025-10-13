#pragma once

#include "exec/executor/Executor.h"
#include "exec/os/OS.h"

#include <supp/IntrusivePriorityQueue.h>
#include <supp/PriorityQueue.h>

#include <time/mono.h>

namespace exec {

template <int MaxTimers, int MaxDefers>
class MicroOS : public OS {
 public:
    explicit MicroOS(Executor* executor) : executor_(executor) {}

    void defer(Runnable* r, ttime::Time at) override {
        defer_.push(Defer{at, r});
    }

    void add(TimerEntry* t) override {
        DASSERT(!t->connected());
        timers_.push(t);
    }

    bool remove(TimerEntry* t) override {
        return timers_.erase(t);
    }

    void tick() {
        auto now = ttime::mono::now();

        while (!defer_.empty() && now >= defer_.front().at) {
            executor_->post(defer_.pop().task);
        }
        while (!timers_.empty() && now >= timers_.front()->at) {
            executor_->post(timers_.pop()->task);
        }
    }

 private:
    struct Defer {  // NOLINT
        ttime::Time at;
        Runnable* task;
    };

    using Comp = decltype([](auto& l, auto& r) { return l.at < r.at; });

    Executor* executor_;
    supp::PriorityQueue<Defer, MaxDefers, Comp> defer_;
    supp::IntrusivePriorityQueue<TimerEntry, MaxTimers, Comp> timers_;
};

}  // namespace exec
