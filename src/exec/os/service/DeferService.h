#pragma once

#include "exec/Runnable.h"
#include "exec/executor/Executor.h"
#include "exec/os/OS.h"

#include <supp/PriorityQueue.h>

#include <time/mono.h>

namespace exec {

class DeferService {
 public:
    virtual ~DeferService() = default;
    virtual void defer(Runnable* r, ttime::Time at) = 0;
};

DeferService* deferService();
void setDeferService(DeferService* s);

template <int MaxDefers>
class HeapDeferService : public DeferService, public Service {
 public:
    HeapDeferService() {
        if (auto o = os()) {
            o->addService(this);
        }

        setDeferService(this);
    }

    void defer(Runnable* r, ttime::Time at) override {
        heap_.push(Defer{at, r});
    }

    void tick() override {
        auto now = ttime::mono::now();

        while (!heap_.empty() && now >= heap_.front().at) {
            executor()->post(heap_.pop().task);
        }
    }

 private:
    struct Defer {  // NOLINT
        ttime::Time at;
        Runnable* task;
    };

    using Comp = decltype([](auto& l, auto& r) { return l.at < r.at; });
    supp::PriorityQueue<Defer, MaxDefers, Comp> heap_;
};

}  // namespace exec
