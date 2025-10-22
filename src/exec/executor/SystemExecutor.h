#pragma once

#include "exec/executor/Executor.h"
#include "exec/os/ServiceBase.h"

#include <supp/IntrusiveForwardList.h>
#include <time/mono.h>

namespace exec {

class SystemExecutor : public Executor, public ServiceBase<Executor, SystemExecutor> {
 public:
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
