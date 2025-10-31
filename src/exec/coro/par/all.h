#pragma once

#include "exec/cancel.h"
#include "exec/coro/traits.h"

#include <array>
#include <coroutine>
#include <cstdint>
#include <cstdlib>
#include <tuple>
#include <utility>

namespace exec {

namespace detail {

template <typename... Ts>
struct AllState : CancellationHandler, supp::Pinned {
    std::coroutine_handle<> arrived() {
        if (--counter == 0) {
            slot_.clearIfConnected();
            return all_waiter;
        }

        return std::noop_coroutine();
    }

    template <size_t I, typename U>
    void returnValue(U&& value) {
        // NOTE: NO MANUAL LIFETIME
        get<I>(result) = std::forward<U>(value);
    }

    bool done() {
        return counter == 0;
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
struct AllTask : supp::Pinned {
    struct promise_type {
        promise_type(auto&& /*task*/, State* state) : state{state} {}

        auto get_return_object() noexcept {
            return AllTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        auto initial_suspend() noexcept {
            return std::suspend_always{};
        }

        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept {
                    return false;
                }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<> /*caller*/) noexcept {
                    // caller is no one
                    return state->arrived();
                }

                void await_resume() noexcept {}

                State* state;
            };

            return FinalAwaiter{state};
        }

        void return_value(T value) {
            state->template returnValue<Index>(std::move(value));
        }

        void unhandled_exception() noexcept {
            LFATAL("unhandled exception in AllTask");
            abort();
        }

        State* state;
    };

    AllTask(std::coroutine_handle<> h) : coro{h} {}
    AllTask(AllTask&& rhs) noexcept : coro{std::exchange(rhs.coro, nullptr)} {}

    ~AllTask() {
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
AllTask<awaitable_result_t<Task>, I, State> makeAllTask(Task&& task, State* /*state*/) {
    // The state goes to promise_type's constructor.
    co_return co_await std::move(task);
}

template <size_t I, typename State, typename Task>
void attachAllTaskCancellation(State* state, Task& task) {
    if constexpr (CancellableAwaitable<Task>) {
        task.setCancellationSlot(state->template getSlot<I>());
    }
}

template <typename State, typename... Tasks>
auto makeAllTasksTuple(State* state, Tasks&&... tasks) {
    return [&]<size_t... Is>(std::index_sequence<Is...>) {
        // Install the cancellation slot before task.start() is called
        (attachAllTaskCancellation<Is>(state, tasks), ...);

        return std::tuple<AllTask<awaitable_result_t<Tasks>, Is, State>...>(
            makeAllTask<Is>(std::forward<Tasks>(tasks), state)...);
    }(std::make_integer_sequence<size_t, sizeof...(Tasks)>());
}

template <typename... Tasks>
struct [[nodiscard]] AllAwaitable : supp::Pinned {
    using StateType = AllState<awaitable_result_t<Tasks>...>;
    using ResultType = std::tuple<awaitable_result_t<Tasks>...>;

    StateType state_;

    using AllTasksTuple = decltype(makeAllTasksTuple(&state_, std::declval<Tasks>()...));

    AllTasksTuple tasks_;

    template <typename... Args>
    AllAwaitable(Args&&... tasks)
        : tasks_{makeAllTasksTuple(&state_, std::forward<Args>(tasks)...)} {}

    bool await_ready() noexcept {
        // start all tasks
        std::apply([](auto&... task) { (task.start(), ...); }, tasks_);

        // suspend only if not not all tasks have been completed
        if (!state_.done()) {
            return false;
        }

        // operation is complete
        state_.slot_.clearIfConnected();
        return true;
    }

    void await_suspend(std::coroutine_handle<> caller) noexcept {
        // All tasks have been started, but not all completed. Register awaiter.
        state_.all_waiter = caller;
    }

    ResultType await_resume() noexcept {
        return std::move(state_.result);
    }

    // CancellableAwaitable
    AllAwaitable& setCancellationSlot(CancellationSlot slot) {
        state_.setCancellationSlot(slot);
        return *this;
    }
};

}  // namespace detail

template <typename... Tasks>
auto all(Tasks&&... tasks) {
    return detail::AllAwaitable<Tasks...>(std::forward<Tasks>(tasks)...);
}

}  // namespace exec
