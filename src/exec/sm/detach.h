#pragma once

#include "exec/executor/Executor.h"
#include "exec/os/Service.h"
#include "exec/sm/Initiator.h"
#include "exec/sm/Task.h"

namespace exec {

template <typename T>
[[nodiscard]] Initiator auto detach(T t) {
    struct Runner : Runnable {
        Runner(T t) : task_{task(std::move(t))} {}  // NOLINT

        Initiator auto start() {
            return [this](Runnable* cb, CancellationSlot slot = {}) mutable {
                cb_ = cb;
                return task_.start()(this, slot);
            };
        }

        // On task completion
        Runnable* run() override {
            auto* cb = cb_;
            delete this;
            return cb;
        }

        TaskType<T> task_;
        Runnable* cb_;
    };

    auto* runner = new Runner(std::move(t));
    return runner->start();
}

template <typename T>
void spawn(T task) {
    auto initiator = detach(std::move(task));
    service<Executor>()->post(initiator(noop));
}

}  // namespace exec
