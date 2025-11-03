#pragma once

#include "exec/Runnable.h"
#include "exec/coro/Async.h"
#include "exec/executor/Executor.h"
#include "exec/os/Service.h"

#include <supp/Pinned.h>

#include <utility>

namespace exec {

namespace detail {

template <typename T>
struct SpawnTask;

template <typename T>
struct SpawnPromise {
    using coroutine_handle_t = std::coroutine_handle<SpawnPromise<T>>;

    auto get_return_object() { return coroutine_handle_t::from_promise(*this); }

    auto initial_suspend() { return std::suspend_always{}; }

    auto final_suspend() const noexcept {
        struct Finalizer {
            constexpr bool await_ready() const noexcept { return false; }
            void await_suspend(coroutine_handle_t coroutine) noexcept {
                delete coroutine.promise().owner_;
            }
            void await_resume() noexcept { DASSERT(false, "implementation bug"); }
        };

        return Finalizer{};
    }

    void return_void() {}

    auto yield_value(auto&& value) {
        (void)value;
        return final_suspend();
    }

    void unhandled_exception() {
        LFATAL("unhandled exception in spawned task");
        abort();
    }

    SpawnTask<T>* owner_ = nullptr;
};

template <typename T>
struct SpawnTask : Runnable, supp::NonCopyable {
 public:
    using promise_type = SpawnPromise<T>;
    using coroutine_handle_t = std::coroutine_handle<promise_type>;

    SpawnTask(coroutine_handle_t coro) : coro_{coro} {}
    SpawnTask(SpawnTask&& r) noexcept : coro_{std::exchange(r.coro_, nullptr)} {}

    ~SpawnTask() {
        if (!coro_) {
            return;
        }

        coro_.destroy();
    }

 private:
    // Runnable
    Runnable* run() override {
        coro_.promise().owner_ = this;
        coro_.resume();
        return noop;
    }

    coroutine_handle_t coro_;
};

template <typename T>
SpawnTask<T> spawn(Async<T> task) {
    co_yield co_await std::move(task);
}

template <typename T>
void launch(SpawnTask<T> task) {
    auto spawn_task_heap = new detail::SpawnTask<T>(std::move(task));
    service<Executor>()->post(spawn_task_heap);
}

}  // namespace detail

template <typename F>
void spawn(F&& task) {
    launch(detail::spawn(task()));
}

template <typename T>
void spawn(Async<T> task) {
    launch(detail::spawn(std::move(task)));
}

}  // namespace exec
