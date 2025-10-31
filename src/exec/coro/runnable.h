#pragma once

#include "exec/Runnable.h"
#include "exec/coro/Async.h"

#include <supp/Pinned.h>

#include <utility>

namespace exec {

template <typename T = Unit>
class RunnableTask : public Runnable, supp::Pinned {
 public:
    RunnableTask(Async<T> task) noexcept : task_{std::move(task)} {}
    RunnableTask(RunnableTask&& r) noexcept = default;

    // Runnable
    // Allows tasks to be launched from the Executor. run() may only be called once.
    Runnable* run() noexcept override {
        task_.resume();
        return noop;
    }

 private:
    Async<T> task_;
};

}  // namespace exec
