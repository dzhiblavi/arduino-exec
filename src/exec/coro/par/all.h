#pragma once

#include "exec/cancel.h"
#include "exec/coro/traits.h"

#include <supp/ManualLifetime.h>
#include <supp/NonCopyable.h>
#include <supp/Pinned.h>

#include <array>
#include <coroutine>
#include <cstdint>
#include <cstdlib>
#include <tuple>
#include <utility>

namespace exec {

namespace detail {

template <typename... Ts>
struct AllState : CancellationHandler {
    static constexpr size_t Children = sizeof...(Ts);

    AllState(CancellationSlot slot) : slot_{slot} {}

    std::coroutine_handle<> arrived() {
        if (--counter == 0) {
            slot_.clearIfConnected();
            return caller;
        }

        return std::noop_coroutine();
    }

    void await_suspend(std::coroutine_handle<> a_caller) {
        slot_.installIfConnected(this);
        caller = a_caller;
    }

    // CancellationHandler
    Runnable* cancel() override {
        DASSERT(caller != nullptr);
        auto a_caller = std::exchange(caller, std::noop_coroutine());

        for (auto& sig : child_signals_) {
            // cannot complete because caller is extracted
            sig.emit();
        }

        if (all_done()) {
            a_caller.resume();
        } else {
            caller = a_caller;
        }

        return noop;
    }

    template <size_t I, typename U>
    void returnValue(U&& value) {
        std::get<I>(result).emplace(std::forward<U>(value));
    }

    template <size_t I>
    CancellationSlot getSlot() {
        return child_signals_[I].slot();
    }

    bool all_done() const { return counter == 0; }

    uint8_t counter = Children;
    std::coroutine_handle<> caller = std::noop_coroutine();
    std::tuple<supp::ManualLifetime<Ts>...> result{};

    CancellationSlot slot_;
    std::array<CancellationSignal, Children> child_signals_;
};

template <typename T, size_t Index, typename State>
struct AllTask : supp::NonCopyable {
    struct promise_type {
        promise_type(auto&& /*task*/, State* state) : state{state} {}

        auto get_return_object() {
            return AllTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        auto initial_suspend() { return std::suspend_always{}; }

        auto final_suspend() const noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<> self) {
                    auto* state_copy = state;
                    self.destroy();
                    return state_copy->arrived();
                }

                void await_resume() noexcept {}

                State* state;
            };

            return FinalAwaiter{state};
        }

        void return_value(T value) { state->template returnValue<Index>(std::move(value)); }

        void unhandled_exception() {
            LFATAL("unhandled exception in AllTask");
            abort();
        }

        State* state;
    };

    AllTask(std::coroutine_handle<> h) : coro{h} {}
    AllTask(AllTask&& rhs) noexcept : coro{std::exchange(rhs.coro, nullptr)} {}

    void start() { coro.resume(); }

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

template <typename... Tasks>
struct [[nodiscard]] All : supp::NonCopyable {
    using StateType = AllState<awaitable_result_t<Tasks>...>;
    using ResultType = std::tuple<awaitable_result_t<Tasks>...>;

    All(Tasks... tasks) : tasks_(std::move(tasks)...) {}
    All(All&&) = default;

    // CancellableAwaitable
    All& setCancellationSlot(CancellationSlot slot) {
        slot_ = slot;
        return *this;
    }

    auto operator co_await() /*&&*/ { return Awaitable{std::move(tasks_), slot_}; }

 private:
    struct Awaitable : supp::Pinned {
        Awaitable(std::tuple<Tasks...>&& tasks, CancellationSlot slot)
            : state_{slot}
            , tasks_(
                  std::apply(
                      [&](auto&&... tasks) { return makeTasks(&state_, std::move(tasks)...); },
                      std::move(tasks))) {}

        bool await_ready() {
            std::apply([](auto&... task) { (task.start(), ...); }, tasks_);
            return state_.all_done();
        }

        void await_suspend(std::coroutine_handle<> caller) { state_.await_suspend(caller); }

        ResultType await_resume() {
            return std::apply(
                [](auto&&... rs) { return ResultType(std::move(rs).get()...); },
                std::move(state_.result));
        }

     private:
        template <size_t I, typename Task>
        static AllTask<awaitable_result_t<Task>, I, StateType> makeTask(
            Task&& task, StateType* /*state*/) {
            // state is passed to the promise_type's constructor
            co_return co_await std::move(task);
        }

        // Sets appropriate cancellation slots to all tasks.
        // Creates co_await'ting AnyTask that will be able to start them.
        // Does not start tasks right away.
        static auto makeTasks(StateType* state, Tasks&&... tasks) {
            return [&]<size_t... Is>(std::index_sequence<Is...>) {
                (attachCancellation<Is>(state, tasks), ...);

                return std::tuple<AllTask<awaitable_result_t<Tasks>, Is, StateType>...>(
                    makeTask<Is>(std::move(tasks), state)...);
            }(std::make_integer_sequence<size_t, sizeof...(Tasks)>());
        }

        // Sets appropriate cancellation slots to Ith child task.
        template <size_t I, typename Task>
        static void attachCancellation(StateType* state, Task& task) {
            if constexpr (CancellableAwaitable<Task>) {
                task.setCancellationSlot(state->template getSlot<I>());
            }
        }

        using TasksTuple =
            decltype(makeTasks(std::declval<StateType*>(), std::declval<Tasks>()...));

        StateType state_;
        TasksTuple tasks_;
    };

    std::tuple<Tasks...> tasks_;
    CancellationSlot slot_{};
};

}  // namespace detail

template <typename... Tasks>
auto all(Tasks... tasks) {
    return detail::All(std::move(tasks)...);
}

}  // namespace exec
