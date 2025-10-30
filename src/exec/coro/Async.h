#pragma once

#include "exec/Unit.h"

#include <supp/ManualLifetime.h>
#include <supp/verify.h>

#include <coroutine>
#include <cstdlib>
#include <utility>

namespace exec {

template <typename T>
class Async;

namespace detail {

class AsyncPromiseBase {
    struct FinalAwaitable {
        constexpr bool await_ready() const noexcept {
            return false;
        }

        template <typename Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> coro) noexcept {
            return coro.promise().continuation_;
        }

        void await_resume() noexcept {}
    };

 public:
    AsyncPromiseBase() noexcept = default;

    auto initial_suspend() noexcept {
        return std::suspend_always{};
    }

    auto final_suspend() noexcept {
        return FinalAwaitable{};
    }

    void unhandled_exception() noexcept {
        LFATAL("unhandled exception in Async body");
        abort();
    }

    void setContinuation(std::coroutine_handle<> continuation) noexcept {
        continuation_ = continuation;
    }

    void* operator new(size_t size) {
        LTRACE("allocating ", size, " bytes for coroutine state");
        return ::operator new(size);
    }

    void operator delete(void* ptr, size_t size) {
        LTRACE("deallocating ", size, " bytes from coroutine state");
        ::operator delete(ptr, size);
    }

 private:
    std::coroutine_handle<> continuation_ = std::noop_coroutine();
};

template <typename T>
class AsyncPromise : public AsyncPromiseBase {
 public:
    AsyncPromise() noexcept = default;
    ~AsyncPromise() = default;

    auto get_return_object() noexcept {  // NOLINT
        return std::coroutine_handle<AsyncPromise>::from_promise(*this);
    }

    template <typename U>
    void return_value(U&& value) noexcept {
        result_.emplace(std::forward<U>(value));
    }

    supp::ManualLifetime<T> result() && {
        return std::move(result_);
    }

 private:
    supp::ManualLifetime<T> result_;
};

template <>
class AsyncPromise<Unit> : public AsyncPromiseBase {
 public:
    AsyncPromise() noexcept = default;
    ~AsyncPromise() = default;

    auto get_return_object() noexcept {
        return std::coroutine_handle<AsyncPromise>::from_promise(*this);
    }

    void return_void() noexcept {
        result_.emplace(unit);
    }

    supp::ManualLifetime<Unit> result() && {
        return std::move(result_);
    }

 private:
    supp::ManualLifetime<Unit> result_;
};

}  // namespace detail

template <typename T = Unit>
class [[nodiscard]] Async {
 public:
    using promise_type = detail::AsyncPromise<T>;
    using value_type = T;

    Async() noexcept = default;
    Async(std::coroutine_handle<promise_type> coroutine) : coroutine_(coroutine) {}
    Async(Async&& t) noexcept : coroutine_(std::exchange(t.coroutine_, nullptr)) {}

    // not copyable
    Async(const Async&) = delete;
    Async& operator=(const Async&) = delete;

    ~Async() {
        if (!coroutine_) {
            // coroutine is destroyed or has never been alive
            return;
        }

        if (!done()) {
            LWARN("destroying a non-finished coroutine");
        }

        coroutine_.destroy();
    }

    void resume() {
        // only suspended, !done coroutines can be resumed
        return coroutine_.resume();
    }

    bool done() const noexcept {
        // only suspended coroutines can be queried
        return coroutine_.done();
    }

    auto operator co_await() && noexcept {
        struct Awaitable {
            bool await_ready() const noexcept {
                return !coroutine || coroutine.done();
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
                coroutine.promise().setContinuation(caller);
                return coroutine;
            }

            T await_resume() noexcept {
                DASSERT(coroutine);

                // pull out the result of the coroutine
                auto res = std::move(coroutine.promise()).result();

                // now we can release the coroutine frame
                std::exchange(coroutine, nullptr).destroy();

                DASSERT(res.initialized(), "nothing returned from an Async<T>");
                return std::move(res).get();
            }

            std::coroutine_handle<promise_type> coroutine;
        };

        // co_await is a destructive operation for Async<T>
        return Awaitable{std::exchange(coroutine_, nullptr)};
    }

 private:
    std::coroutine_handle<promise_type> coroutine_;
};

}  // namespace exec
