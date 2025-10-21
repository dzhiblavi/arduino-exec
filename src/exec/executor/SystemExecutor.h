#pragma once

#include "exec/executor/Executor.h"
#include "exec/os/OS.h"

#include <time/mono.h>
#include <supp/IntrusiveForwardList.h>

namespace exec {

class SystemExecutor : public Executor, public Service {
 public:
    SystemExecutor() {
        if (auto o = os()) {
            o->addService(this);
        }

        setExecutor(this);
    }

    void post(Runnable* r) override {
        queue_.pushBack(r);
    }

    // Service
    void tick() override {
        auto q = std::move(queue_);

        while (!q.empty()) {
            q.popFront()->runAll();
        }

        queue_.prepend(std::move(q));
    }

    ttime::Time wakeAt() const override {
        return queue_.empty() ? ttime::Time::max() : ttime::mono::now();
    }

 private:
    supp::IntrusiveForwardList<Runnable> queue_;
};

}  // namespace exec
