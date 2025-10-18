#pragma once

#include "exec/executor/Executor.h"
#include "exec/os/OS.h"

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

    void tick() override {
        auto q = std::move(queue_);

        while (!q.empty()) {
            q.popFront()->runAll();
        }

        queue_.prepend(std::move(q));
    }

 private:
    supp::IntrusiveForwardList<Runnable> queue_;
};

}  // namespace exec
