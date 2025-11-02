#pragma once

#include "exec/Unit.h"
#include "exec/cancel.h"
#include "exec/coro/traits.h"

#include <supp/ManualLifetime.h>
#include <supp/NonCopyable.h>
#include <supp/verify.h>

#include <coroutine>
#include <cstdlib>
#include <utility>

namespace exec {

template <typename T>
class Async;

inline constexpr struct extract_cancellation_slot_t {
} extract_cancellation_slot{};

struct replace_cancellation_slot {
    CancellationSlot slot;
};

namespace detail {

class AsyncPromiseBase {
    struct FinalAwaitable {
        constexpr bool await_ready() const noexcept {
            return false;
        }

        template <typename Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> self) noexcept {
            auto continuation = self.promise().continuation_;
            self.destroy();
            return continuation;
        }

        void await_resume() noexcept {}
    };

    struct ExtractCancellationSlotAwaitable {
        constexpr bool await_ready() const {
            return true;
        }

        constexpr void await_suspend(std::coroutine_handle<>) const {}

        CancellationSlot await_resume() const {
            return slot;
        }

        CancellationSlot slot;
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

    template <CancellableAwaitable A>
    decltype(auto) await_transform(A&& awaitable) {
        if (slot_.isConnected()) {
            // Connect cancellation when awaiting cancellable awaitables
            awaitable.setCancellationSlot(slot_);
        }

        return std::forward<A>(awaitable);
    }

    template <typename A>
    decltype(auto) await_transform(A&& awaitable) {
        return std::forward<A>(awaitable);
    }

    auto await_transform(extract_cancellation_slot_t) {
        return ExtractCancellationSlotAwaitable{std::move(slot_)};
    }

    auto await_transform(replace_cancellation_slot r) {
        auto extracted = std::move(slot_);
        slot_ = std::move(r.slot);
        return ExtractCancellationSlotAwaitable{std::move(extracted)};
    }

    void setContinuation(std::coroutine_handle<> continuation) noexcept {
        continuation_ = continuation;
    }

    // CancellableAwaitable
    void setCancellationSlot(CancellationSlot slot) noexcept {
        slot_ = slot;
    }

    void* operator new(size_t size) {
        return ::operator new(size);
    }

    void operator delete(void* ptr, size_t size) {
        ::operator delete(ptr, size);
    }

 private:
    std::coroutine_handle<> continuation_ = std::noop_coroutine();
    CancellationSlot slot_;
};

template <typename T>
class AsyncPromise : public AsyncPromiseBase {
 public:
    AsyncPromise() noexcept = default;

    auto get_return_object() noexcept {  // NOLINT
        return std::coroutine_handle<AsyncPromise>::from_promise(*this);
    }

    template <typename U>
    void return_value(U&& value) noexcept {
        DASSERT(result_);
        result_->emplace(std::forward<U>(value));
    }

    void setResultPtr(supp::ManualLifetime<T>* ptr) {
        result_ = ptr;
    }

 private:
    supp::ManualLifetime<T>* result_ = nullptr;
};

template <>
class AsyncPromise<Unit> : public AsyncPromiseBase {
 public:
    AsyncPromise() noexcept = default;

    auto get_return_object() noexcept {
        return std::coroutine_handle<AsyncPromise>::from_promise(*this);
    }

    void return_void() noexcept {
        DASSERT(result_);
        result_->emplace(unit);
    }

    void setResultPtr(supp::ManualLifetime<Unit>* ptr) {
        result_ = ptr;
    }

 private:
    supp::ManualLifetime<Unit>* result_ = nullptr;
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

    ~Async() noexcept {
        DASSERT(!coroutine_, F("Async<T> has not been consumed"));
    }

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

            bool await_ready() const noexcept {
                return coroutine_.done();
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
                auto& promise = coroutine_.promise();
                promise.setResultPtr(&result_);
                promise.setContinuation(caller);
                return coroutine_;
            }

            T await_resume() noexcept {
                DASSERT(result_.initialized(), "nothing returned from an Async<T>");
                return std::move(result_).get();
            }

            std::coroutine_handle<promise_type> coroutine_;
            supp::ManualLifetime<T> result_{};
        };

        // co_await is a destructive operation for Async<T>
        DASSERT(coroutine_, "Async<T> has been consumed");
        return Awaitable{std::exchange(coroutine_, nullptr)};
    }

 private:
    std::coroutine_handle<promise_type> coroutine_ = nullptr;
};

}  // namespace exec
