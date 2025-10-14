#pragma once

#include "exec/executor/Executor.h"
#include "exec/os/Service.h"

#include <supp/IntrusiveForwardList.h>

namespace exec {

class SystemExecutor : public Executor, public Service {
 public:
    void post(Runnable* r) override {
        queue_.pushBack(r);
    }

    void tick() override {
        auto q = stdlike::move(queue_);

        while (!q.empty()) {
            q.popFront()->runAll();
        }

        queue_.prepend(stdlike::move(q));
    }

 private:
    supp::IntrusiveForwardList<Runnable> queue_;
};

}  // namespace exec
