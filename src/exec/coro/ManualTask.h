#pragma once

#include "exec/coro/Async.h"

#include <supp/Pinned.h>

#include <utility>

namespace exec {

template <typename T>
class ManualTask;

namespace detail {

template <typename T>
struct ManualPromise {
 public:
    auto get_return_object() {
        return std::coroutine_handle<ManualPromise<T>>::from_promise(*this);
    }

    auto initial_suspend() const { return std::suspend_always{}; }

    auto final_suspend() const noexcept {
        struct Awaitable {
            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<> self) noexcept { self.destroy(); }
            void await_resume() noexcept {}
        };
        return Awaitable{};
    }

    void return_value(Result<T> res) const { *result_ = std::move(res); }

    void unhandled_exception() const {
        LFATAL("unhandled_exception");
        abort();
    }

 private:
    Result<T>* result_ = nullptr;

    friend class ManualTask<T>;
};

}  // namespace detail

template <typename T>
class ManualTask : supp::NonCopyable {
 public:
    using promise_type = detail::ManualPromise<T>;
    using coroutine_handle_t = std::coroutine_handle<promise_type>;

    ManualTask(coroutine_handle_t coro) : coroutine_{coro} {}
    ManualTask(ManualTask&& r) noexcept : coroutine_{std::exchange(r.coroutine_, nullptr)} {}

    ~ManualTask() {
        if (!coroutine_) {
            return;
        }

        DASSERT(done(), "task destroyed before completion");
    }

    void start() {
        DASSERT(coroutine_);
        coroutine_.promise().result_ = &result_;
        coroutine_.resume();
    }

    bool done() const { return result_.code() != ErrCode::Unknown; }

    const Result<T>& result() const {
        DASSERT(done());
        return result_;
    }

 private:
    coroutine_handle_t coroutine_;
    Result<T> result_;

    friend struct detail::ManualPromise<T>;
};

template <typename T>
ManualTask<T> makeManualTask(Async<T> task) {
    co_return co_await std::move(task);
}

}  // namespace exec
