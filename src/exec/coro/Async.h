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

inline constexpr struct cancellation_state_t {
} cancellation_state;

namespace detail {

template <typename T>
class AsyncPromiseBase : CancellationHandler {
    template <typename P>
    static std::coroutine_handle<> finalSuspend(std::coroutine_handle<P> self) {
        auto& promise = self.promise();
        promise.up_slot_.clearIfConnected();

        auto continuation = promise.continuation_;
        self.destroy();
        return continuation;
    }

    struct FinalAwaitable {
        constexpr bool await_ready() const noexcept { return false; }

        template <typename P>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<P> self) {
            return finalSuspend(self);
        }

        void await_resume() noexcept {}
    };

    template <typename A>
    struct Callee {
        bool await_ready() { return !cancelled && impl.await_ready(); }

        template <typename P>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<P> self_p) {
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

        decltype(auto) await_resume() {
            DASSERT(!cancelled, "implementation bug");
            return std::move(impl).await_resume();
        }

        const bool cancelled;
        A impl;
    };

    struct IgnoreCancellationGuard : supp::NonCopyable {
        IgnoreCancellationGuard(AsyncPromiseBase* self) : self{self} {
            self->up_slot_.clearIfConnected();
        }

        IgnoreCancellationGuard(IgnoreCancellationGuard&& rhs) noexcept
            : self{std::exchange(rhs.self, nullptr)} {}

        ~IgnoreCancellationGuard() {
            if (self) {
                self->up_slot_.installIfConnected(self);
            }
        }

        AsyncPromiseBase* self;
    };

    struct IgnoreCancellationAwaitable {
        bool await_ready() const { return true; }
        void await_suspend(std::coroutine_handle<>) const {}
        IgnoreCancellationGuard await_resume() const { return {self}; }
        AsyncPromiseBase* self;
    };

    struct CancellationStateAwaitable {
        bool await_ready() const { return true; }
        void await_suspend(std::coroutine_handle<>) const {}
        bool await_resume() const { return self->cancelled(); }
        AsyncPromiseBase* self;
    };

 public:
    AsyncPromiseBase() = default;

    auto initial_suspend() { return std::suspend_always{}; }
    auto final_suspend() noexcept { return FinalAwaitable{}; }

    void unhandled_exception() {
        LFATAL("unhandled exception in Async body");
        abort();
    }

    template <typename A>
    auto await_transform(A&& awaitable) {
        if constexpr (CancellableAwaitable<A>) {
            if (up_slot_.isConnected()) [[likely]] {
                awaitable.setCancellationSlot(down_sig_.slot());
            }
        }

        return Callee<get_awaiter_t<A>>(
            cancelled(), std::forward<A>(awaitable).operator co_await());
    }

    auto await_transform(ignore_cancellation_t) { return IgnoreCancellationAwaitable{this}; }
    auto await_transform(cancellation_state_t) { return CancellationStateAwaitable{this}; }

    void setCancellationSlot(CancellationSlot slot) { up_slot_ = slot; }

    void suspend(std::coroutine_handle<> caller, Result<T>* result) {
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
    std::coroutine_handle<> cancel() override {
        result_->setError(ErrCode::Cancelled);
        return down_sig_.emit();
    }

    bool cancelled() const { return result_->code() == ErrCode::Cancelled; }

    CancellationSlot up_slot_;
    CancellationSignal down_sig_;
    std::coroutine_handle<> continuation_ = std::noop_coroutine();
};

template <typename T>
class AsyncPromise : public AsyncPromiseBase<T> {
 public:
    AsyncPromise() = default;

    auto get_return_object() {  // NOLINT
        return std::coroutine_handle<AsyncPromise>::from_promise(*this);
    }

    template <typename U>
    void return_value(U&& value) {
        DASSERT(this->result_);
        this->result_->emplace(std::forward<U>(value));
    }

    void return_value(Result<T> value) {
        DASSERT(this->result_);
        *this->result_ = std::move(value);
    }
};

template <>
class AsyncPromise<Unit> : public AsyncPromiseBase<Unit> {
 public:
    AsyncPromise() = default;

    auto get_return_object() { return std::coroutine_handle<AsyncPromise>::from_promise(*this); }

    void return_void() {
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

    Async() = default;
    Async(std::coroutine_handle<promise_type> coroutine) : coroutine_(coroutine) {}
    Async(Async&& r) noexcept : coroutine_(std::exchange(r.coroutine_, nullptr)) {}

    ~Async() {
        if (!coroutine_) {
            return;
        }

        // Async<T> has not been consumed
        coroutine_.destroy();
    }

    // CancellableAwaitable
    Async& setCancellationSlot(CancellationSlot slot) & {
        DASSERT(coroutine_, F("Async<T> has been consumed"));
        coroutine_.promise().setCancellationSlot(slot);
        return *this;
    }

    // CancellableAwaitable
    Async setCancellationSlot(CancellationSlot slot) && {
        DASSERT(coroutine_, F("Async<T> has been consumed"));
        coroutine_.promise().setCancellationSlot(slot);
        return std::move(*this);
    }

    auto operator co_await() {
        struct Awaitable : supp::Pinned {
            Awaitable(std::coroutine_handle<promise_type> coroutine)
                : coroutine_{std::move(coroutine)} {}

            ~Awaitable() {
                if (!coroutine_) {
                    return;
                }

                // the coroutine has been discarded
                coroutine_.destroy();
            }

            bool await_ready() {
                if (!coroutine_) {
                    result_.setError(ErrCode::OutOfMemory);
                    return true;
                }

                return coroutine_.done();
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) {
                coroutine_.promise().suspend(caller, &result_);
                return std::exchange(coroutine_, nullptr);
            }

            Result<T> await_resume() { return std::move(result_); }

            std::coroutine_handle<promise_type> coroutine_;
            Result<T> result_;
        };

        // nullptr coroutine_ means OutOfMemory
        return Awaitable{std::exchange(coroutine_, nullptr)};
    }

 private:
    std::coroutine_handle<promise_type> coroutine_ = nullptr;
};

template <typename T>
class Async<Result<T>> {};

}  // namespace exec
