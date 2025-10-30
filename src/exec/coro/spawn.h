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

    auto get_return_object() noexcept {
        return coroutine_handle_t::from_promise(*this);
    }

    auto initial_suspend() noexcept {
        return std::suspend_always{};
    }

    auto final_suspend() noexcept {
        struct Finalizer {
            constexpr bool await_ready() const noexcept {
                return false;
            }

            void await_suspend(coroutine_handle_t coroutine) noexcept {
                LTRACE("deallocating SpawnTask");
                delete coroutine.promise().owner_;
            }

            void await_resume() noexcept {
                __builtin_unreachable();
            }
        };

        return Finalizer{};
    }

    void return_void() noexcept {}

    auto yield_value(T value) noexcept {
        (void)value;
        return final_suspend();
    }

    void unhandled_exception() noexcept {
        LFATAL("unhandled exception in spawned task");
        abort();
    }

    SpawnTask<T>* owner_ = nullptr;
};

template <typename T>
struct SpawnTask : Runnable, supp::Pinned {
    using promise_type = SpawnPromise<T>;
    using coroutine_handle_t = std::coroutine_handle<promise_type>;

    SpawnTask(coroutine_handle_t coro) noexcept : coro_{coro} {}
    SpawnTask(SpawnTask&& r) noexcept : coro_{std::exchange(r.coro_, nullptr)} {}

    // not copyable
    SpawnTask(const SpawnTask&) = delete;
    SpawnTask& operator=(const SpawnTask&) = delete;

    ~SpawnTask() noexcept {
        if (!coro_) {
            return;
        }

        coro_.destroy();
    }

    // Runnable
    Runnable* run() noexcept override {
        coro_.promise().owner_ = this;
        coro_.resume();
        return noop;
    }

    coroutine_handle_t coro_;
};

template <typename T>
SpawnTask<T> spawn(Async<T> task) noexcept {
    co_yield co_await std::move(task);
}

template <typename T>
void launch(SpawnTask<T> task) {
    LTRACE("allocating SpawnTask");
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
