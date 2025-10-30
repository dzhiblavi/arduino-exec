#pragma once

#include "exec/cancel.h"
#include "exec/coro/traits.h"

#include <supp/tuple.h>

#include <array>
#include <coroutine>
#include <tuple>
#include <utility>

#include <cstdint>
#include <cstdlib>

namespace exec {

namespace detail {

// CancellableAwaitable
// Cancels all child tasks when the first one completes, then waits for all of them to complete.
template <typename... Ts>
struct AnyState : CancellationHandler {
    std::coroutine_handle<> arrived() {
        auto cur_counter = --counter;

        if (all_waiter != std::noop_coroutine() && cur_counter == sizeof...(Ts) - 1) {
            // Cancel only in both statements are true:
            // 1. all_waiter is present. That means that all tasks have been started
            //    and its OK to call their cancellation.
            // 2. Current task is the first that has completed.
            cancel();
        }

        // do not resume until all tasks are successfully cancelled
        return cur_counter == 0 ? all_waiter : std::noop_coroutine();
    }

    template <size_t I, typename U>
    void returnValue(U&& value) {
        // NOTE: NO MANUAL LIFETIME
        get<I>(result) = std::forward<U>(value);
    }

    bool done() {
        return counter < sizeof...(Ts);
    }

    void setCancellationSlot(CancellationSlot slot) {
        slot_ = slot;
        slot_.installIfConnected(this);
    }

    // CancellationHandler
    Runnable* cancel() override {
        slot_.clearIfConnected();

        for (auto& sig : child_signals_) {
            sig.emit();
        }

        return noop;
    }

    template <size_t I>
    CancellationSlot getSlot() {
        return child_signals_[I].slot();
    }

    uint8_t counter = sizeof...(Ts);
    std::coroutine_handle<> all_waiter = std::noop_coroutine();
    std::tuple<Ts...> result{};

    CancellationSlot slot_;
    std::array<CancellationSignal, sizeof...(Ts)> child_signals_;
};

template <typename T, size_t Index, typename State>
struct AnyTask {
    struct promise_type {
        promise_type(auto& /*task*/, State* state) : state{state} {}

        auto get_return_object() noexcept {
            return AnyTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        auto initial_suspend() noexcept {
            return std::suspend_always{};
        }

        auto final_suspend() noexcept {
            struct FinalAwaiter {
                constexpr bool await_ready() const noexcept {
                    return false;
                }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<> /*caller*/) noexcept {
                    // Caller is to be ignored.
                    return state->arrived();
                }

                constexpr void await_resume() const noexcept {}

                State* state;
            };

            return FinalAwaiter{state};
        }

        void return_value(T value) {
            state->template returnValue<Index>(std::move(value));
        }

        void unhandled_exception() noexcept {
            LFATAL("unhandled exception in AnyTask");
            abort();
        }

        State* state;
    };

    AnyTask(std::coroutine_handle<> h) : coro{h} {}
    AnyTask(AnyTask&& r) noexcept : coro{std::exchange(r.coro, nullptr)} {}

    // not copyable
    AnyTask(const AnyTask&) = delete;
    AnyTask& operator=(const AnyTask&) = delete;

    ~AnyTask() {
        if (!coro) {
            return;
        }

        coro.destroy();
    }

    void start() {
        coro.resume();
    }

    std::coroutine_handle<> coro;
};

template <size_t I, typename Task, typename State>
AnyTask<awaitable_result_t<Task>, I, State> makeAnyTask(Task&& task, State* /*state*/) {
    // The state goes to promise_type's constructor.
    co_return co_await std::move(task);
}

template <size_t I, typename State, typename Task>
void attachCancellation(State* state, Task& task) {
    if constexpr (CancellableAwaitable<Task>) {
        task.setCancellationSlot(state->template getSlot<I>());
    }
}

template <typename State, typename... Tasks>
auto makeAnyTasksTuple(State* state, Tasks&&... tasks) {
    return [&]<size_t... Is>(std::index_sequence<Is...>) {
        // Install the cancellation slot before task.start() is called
        (attachCancellation<Is>(state, tasks), ...);

        return std::tuple<AnyTask<awaitable_result_t<Tasks>, Is, State>...>(
            makeAnyTask<Is>(std::forward<Tasks>(tasks), state)...);
    }(std::make_integer_sequence<size_t, sizeof...(Tasks)>());
}

template <typename... Tasks>
struct AnyAwaitable : supp::Pinned {
    using StateType = AnyState<awaitable_result_t<Tasks>...>;
    using ResultType = std::tuple<awaitable_result_t<Tasks>...>;

    StateType state_;

    using AnyTasksTuple = decltype(makeAnyTasksTuple(&state_, std::declval<Tasks>()...));

    AnyTasksTuple tasks_;

    AnyAwaitable(Tasks... tasks)
        : tasks_{makeAnyTasksTuple(&state_, std::forward<Tasks>(tasks)...)} {}

    bool await_ready() noexcept {
        // start all tasks
        std::apply([](auto&... task) { (task.start(), ...); }, tasks_);

        // suspend only if not done
        if (!state_.done()) {
            return false;
        }

        // operation is complete, cancel all other tasks
        state_.cancel();
        state_.slot_.clearIfConnected();
        return true;
    }

    void await_suspend(std::coroutine_handle<> caller) noexcept {
        // All tasks have been started, but none of them completed. Register awaiter.
        state_.all_waiter = caller;
    }

    ResultType await_resume() noexcept {
        return std::move(state_.result);
    }

    // CancellableAwaitable
    AnyAwaitable& setCancellationSlot(CancellationSlot slot) {
        state_.setCancellationSlot(slot);
        return *this;
    }
};

}  // namespace detail

template <typename... Tasks>
auto any(Tasks&&... tasks) {
    return detail::AnyAwaitable<Tasks&&...>(std::forward<Tasks>(tasks)...);
}

}  // namespace exec
