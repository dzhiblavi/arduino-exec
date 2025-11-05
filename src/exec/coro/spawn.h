#pragma once

#include "exec/Runnable.h"
#include "exec/coro/traits.h"
#include "exec/executor/Executor.h"
#include "exec/os/Service.h"

#include <supp/NonCopyable.h>

#include <coroutine>
#include <utility>

namespace exec {

namespace detail {

template <typename T>
struct SpawnPromise : Runnable {
    using coroutine_handle_t = std::coroutine_handle<SpawnPromise<T>>;

    coroutine_handle_t handle() { return coroutine_handle_t::from_promise(*this); }
    auto get_return_object() { return handle(); }
    auto initial_suspend() { return std::suspend_always{}; }

    auto final_suspend() const noexcept {
        struct Finalizer {
            constexpr bool await_ready() const noexcept { return false; }
            void await_suspend(coroutine_handle_t coroutine) noexcept { coroutine.destroy(); }
            void await_resume() noexcept { DASSERT(false, "implementation bug"); }
        };

        return Finalizer{};
    }

    template <typename U>
    auto return_value(U&& value) {
        (void)value;
        return final_suspend();
    }

    void unhandled_exception() {
        LFATAL("unhandled exception in spawned task");
        abort();
    }

    // Runnable
    Runnable* run() override {
        handle().resume();
        return noop;
    }
};

template <typename T>
struct SpawnTask : supp::NonCopyable {
 public:
    using promise_type = SpawnPromise<T>;
    using coroutine_handle_t = std::coroutine_handle<promise_type>;

    SpawnTask(coroutine_handle_t coro) : coroutine_{coro} {}
    SpawnTask(SpawnTask&& r) noexcept : coroutine_{std::exchange(r.coroutine_, nullptr)} {}
    ~SpawnTask() { DASSERT(!coroutine_, F("SpawnTask<T> not spawned")); }

    SpawnPromise<T>* promise() && { return &std::exchange(coroutine_, nullptr).promise(); }

 private:
    coroutine_handle_t coroutine_;
};

template <Awaitable A>
SpawnTask<awaitable_result_t<A>> spawn(A task) {
    co_return co_await std::move(task);
}

}  // namespace detail

template <Awaitable A>
void spawn(A&& awaitable) {
    service<Executor>()->post(detail::spawn(std::forward<A>(awaitable)).promise());
}

}  // namespace exec
