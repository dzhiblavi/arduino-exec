#pragma once

#include "exec/Result.h"
#include "exec/Unit.h"
#include "exec/cancel.h"

#include "exec/coro/alloc.h"
#include "exec/coro/traits.h"

#include <supp/ManualLifetime.h>
#include <supp/NonCopyable.h>
#include <supp/Pinned.h>
#include <supp/verify.h>

#include <coroutine>
#include <cstdlib>
#include <utility>

namespace exec {

template <typename T>
class Async;

inline constexpr struct ignore_cancellation_t {
} ignore_cancellation;

namespace detail {

template <typename T>
class AsyncPromiseBase : CancellationHandler {
    template <typename P>
    static std::coroutine_handle<> finalSuspend(std::coroutine_handle<P> self) noexcept {
        auto& promise = self.promise();
        promise.up_slot_.clearIfConnected();

        auto continuation = promise.continuation_;
        self.destroy();
        return continuation;
    }

    struct FinalAwaitable {
        constexpr bool await_ready() const noexcept { return false; }

        template <typename P>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<P> self) noexcept {
            return finalSuspend(self);
        }

        void await_resume() noexcept {}
    };

    template <typename A>
    struct Callee {
        bool await_ready() noexcept { return !cancelled && impl.await_ready(); }

        template <typename P>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<P> self_p) noexcept {
            if (cancelled) {
                return finalSuspend(self_p);
            }

            if constexpr (std::same_as<void, decltype(impl.await_suspend(self_p))>) {
                impl.await_suspend(self_p);
                return std::noop_coroutine();
            } else {
                return impl.await_suspend(self_p);
            }
        }

        decltype(auto) await_resume() noexcept {
            DASSERT(!cancelled, "implementation bug");
            return std::move(impl).await_resume();
        }

        const bool cancelled;
        A impl;
    };

    struct IgnoreCancellationGuard : supp::NonCopyable {
        IgnoreCancellationGuard(IgnoreCancellationGuard&& rhs) noexcept
            : self{std::exchange(rhs.self, nullptr)} {}

        IgnoreCancellationGuard(AsyncPromiseBase* self) noexcept : self{self} {
            self->up_slot_.clearIfConnected();
        }

        ~IgnoreCancellationGuard() noexcept {
            if (self) {
                self->up_slot_.installIfConnected(self);
            }
        }

        AsyncPromiseBase* self;
    };

    struct IgnoreCancellationAwaitable {
        bool await_ready() const noexcept { return true; }
        void await_suspend(std::coroutine_handle<>) const noexcept {}
        IgnoreCancellationGuard await_resume() const noexcept { return {self}; }
        AsyncPromiseBase* self;
    };

 public:
    AsyncPromiseBase() noexcept = default;

    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return FinalAwaitable{}; }

    void unhandled_exception() noexcept {
        LFATAL("unhandled exception in Async body");
        abort();
    }

    template <typename A>
    auto await_transform(A&& awaitable) noexcept {
        return std::forward<A>(awaitable);
    }

    template <CancellableAwaitable A>
    auto await_transform(A&& awaitable) {
        if (up_slot_.isConnected()) [[likely]] {
            // connect the downstream cancellation chain
            awaitable.setCancellationSlot(down_sig_.slot());
        }

        return Callee<get_awaiter_t<A>>(
            cancelled(), std::forward<A>(awaitable).operator co_await());
    }

    auto await_transform(ignore_cancellation_t) noexcept {
        return IgnoreCancellationAwaitable{this};
    }

    void setCancellationSlot(CancellationSlot slot) noexcept { up_slot_ = slot; }

    void suspend(std::coroutine_handle<> caller, Result<T>* result) noexcept {
        continuation_ = caller;
        result_ = result;
        up_slot_.installIfConnected(this);
    }

    void* operator new(size_t size) noexcept { return alloc::allocate(size, std::nothrow); }
    void operator delete(void* ptr, size_t size) { alloc::deallocate(ptr, size); }
    static auto get_return_object_on_allocation_failure() { return Async<T>{}; }

 protected:
    Result<T>* result_ = nullptr;

 private:
    // CancellationHandler
    Runnable* cancel() noexcept override {
        up_slot_.clearIfConnected();
        result_->setError(ErrCode::Cancelled);
        down_sig_.emit();
        return noop;
    }

    bool cancelled() const noexcept { return result_->code() == ErrCode::Cancelled; }

    CancellationSlot up_slot_;
    CancellationSignal down_sig_;
    std::coroutine_handle<> continuation_ = std::noop_coroutine();
};

template <typename T>
class AsyncPromise : public AsyncPromiseBase<T> {
 public:
    AsyncPromise() noexcept = default;

    auto get_return_object() noexcept {  // NOLINT
        return std::coroutine_handle<AsyncPromise>::from_promise(*this);
    }

    template <typename U>
    void return_value(U&& value) noexcept {
        DASSERT(this->result_);
        this->result_->emplace(std::forward<U>(value));
    }

    void return_value(Result<T> value) noexcept {
        DASSERT(this->result_);
        *this->result_ = std::move(value);
    }
};

template <>
class AsyncPromise<Unit> : public AsyncPromiseBase<Unit> {
 public:
    AsyncPromise() noexcept = default;

    auto get_return_object() noexcept {
        return std::coroutine_handle<AsyncPromise>::from_promise(*this);
    }

    void return_void() noexcept {
        DASSERT(this->result_);
        this->result_->emplace(unit);
    }
};

}  // namespace detail

template <typename T = Unit>
class [[nodiscard]] Async : supp::NonCopyable {
 public:
    using promise_type = detail::AsyncPromise<T>;
    using value_type = T;

    Async() noexcept = default;
    Async(std::coroutine_handle<promise_type> coroutine) : coroutine_(coroutine) {}
    Async(Async&& t) noexcept : coroutine_(std::exchange(t.coroutine_, nullptr)) {}
    ~Async() noexcept { DASSERT(!coroutine_, F("Async<T> has not been consumed")); }

    // CancellableAwaitable
    Async& setCancellationSlot(CancellationSlot slot) & noexcept {
        DASSERT(coroutine_, F("Async<T> has been consumed"));
        coroutine_.promise().setCancellationSlot(slot);
        return *this;
    }

    // CancellableAwaitable
    Async setCancellationSlot(CancellationSlot slot) && noexcept {
        DASSERT(coroutine_, F("Async<T> has been consumed"));
        coroutine_.promise().setCancellationSlot(slot);
        return std::move(*this);
    }

    auto operator co_await() noexcept {
        struct Awaitable : supp::Pinned {
            Awaitable(std::coroutine_handle<promise_type> coroutine)
                : coroutine_{std::move(coroutine)} {}

            bool await_ready() const noexcept { return !coroutine_ || coroutine_.done(); }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
                DASSERT(coroutine_);
                auto& promise = coroutine_.promise();
                promise.suspend(caller, &result_);
                return coroutine_;
            }

            Result<T> await_resume() noexcept {
                if (!coroutine_) {
                    result_.setError(ErrCode::OutOfMemory);
                }

                return std::move(result_);
            }

            std::coroutine_handle<promise_type> coroutine_;
            Result<T> result_;
        };

        // nullptr coroutine means OutOfMemory
        return Awaitable{std::exchange(coroutine_, nullptr)};
    }

 private:
    std::coroutine_handle<promise_type> coroutine_ = nullptr;
};

}  // namespace exec
