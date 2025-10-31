#pragma once

#include "exec/coro/Async.h"

#include <supp/Pinned.h>

#include <utility>

namespace exec {

namespace detail {

template <typename T>
struct ManualPromise {
 public:
    auto get_return_object() {
        return std::coroutine_handle<ManualPromise<T>>::from_promise(*this);
    }

    auto initial_suspend() const {
        return std::suspend_always{};
    }

    auto final_suspend() const noexcept {
        // allow user to retrieve the result
        return std::suspend_always{};
    }

    void unhandled_exception() const noexcept {
        LFATAL("unhandled_exception");
        abort();
    }

    template <typename U>
    void return_value(U&& value) {
        result_.emplace(std::forward<U>(value));
    }

    const supp::ManualLifetime<T>& result() const {
        return result_;
    }

 private:
    supp::ManualLifetime<T> result_;
};

}  // namespace detail

template <typename T>
class ManualTask {
 public:
    using promise_type = detail::ManualPromise<T>;
    using coroutine_handle_t = std::coroutine_handle<promise_type>;

    ManualTask(coroutine_handle_t coro) : coroutine_{coro} {}
    ManualTask(ManualTask&& r) noexcept : coroutine_{std::exchange(r.coroutine_, nullptr)} {}

    // not copyable
    ManualTask(const ManualTask&) = delete;
    ManualTask& operator=(const ManualTask&) = delete;

    ~ManualTask() noexcept {
        if (!coroutine_) {
            return;
        }

        DASSERT(coroutine_.done(), "task destroyed before completion");
        coroutine_.destroy();
    }

    void start() {
        DASSERT(coroutine_);
        coroutine_.resume();
    }

    bool done() const {
        DASSERT(coroutine_);
        return coroutine_.done();
    }

    supp::ManualLifetime<T> result() const {
        DASSERT(coroutine_);
        return coroutine_.promise().result();
    }

 private:
    coroutine_handle_t coroutine_;
};

template <typename T>
ManualTask<T> makeManualTask(Async<T> task) {
    co_return co_await std::move(task);
}

}  // namespace exec
