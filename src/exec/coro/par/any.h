#pragma once

#include "exec/coro/cancel.h"
#include "exec/coro/traits.h"

#include <supp/ManualLifetime.h>
#include <supp/NonCopyable.h>
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
    static constexpr size_t Children = sizeof...(Ts);

    AnyState(CancellationSlot slot) : slot_{slot} {}

    std::coroutine_handle<> arrived() {
        const size_t cur_counter = --counter;

        if (caller == nullptr) {
            return std::noop_coroutine();
        }

        const bool first = cur_counter == Children - 1;
        if (first) {
            return doCancel();
        }

        return cur_counter == 0 ? std::exchange(caller, nullptr) : std::noop_coroutine();
    }

    bool await_ready() {
        if (!any_done()) {
            return false;
        }

        for (auto& sig : child_signals_) {
            sig.emitSync();
        }

        return all_done();
    }

    void await_suspend(std::coroutine_handle<> a_caller) {
        if (!any_done()) {
            slot_.installIfConnected(this);
        }

        caller = a_caller;
    }

    // CancellationHandler
    std::coroutine_handle<> cancel() override { return doCancel(); }

    std::coroutine_handle<> doCancel() {
        DASSERT(caller != nullptr);
        auto a_caller = std::exchange(caller, nullptr);

        for (auto& sig : child_signals_) {
            // cannot complete because caller is extracted
            sig.emitSync();
        }

        if (all_done()) {
            return a_caller;
        }

        caller = a_caller;
        return std::noop_coroutine();
    }

    template <size_t I, typename U>
    void returnValue(U&& value) {
        std::get<I>(result).emplace(std::forward<U>(value));
    }

    template <size_t I>
    CancellationSlot getSlot() {
        return child_signals_[I].slot();
    }

    bool any_done() const { return counter < Children; }
    bool all_done() const { return counter == 0; }

    uint8_t counter = Children;
    std::coroutine_handle<> caller = nullptr;
    std::tuple<supp::ManualLifetime<Ts>...> result{};

    CancellationSlot slot_;
    std::array<CancellationSignal, Children> child_signals_;
};

template <typename T, size_t Index, typename State>
struct AnyTask : supp::NonCopyable {
    struct promise_type {
        promise_type(auto&& /*task*/, State* state) : state{state} {}

        auto get_return_object() {
            return AnyTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        auto initial_suspend() const { return std::suspend_always{}; }

        auto final_suspend() const noexcept {
            struct FinalAwaiter {
                constexpr bool await_ready() const noexcept { return false; }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<> self) {
                    auto* state_copy = state;
                    self.destroy();
                    return state_copy->arrived();
                }

                constexpr void await_resume() const noexcept {}

                State* state;
            };

            return FinalAwaiter{state};
        }

        void return_value(T value) { state->template returnValue<Index>(std::move(value)); }

        void unhandled_exception() {
            LFATAL("unhandled exception in AnyTask");
            abort();
        }

        State* state;
    };

    AnyTask(std::coroutine_handle<> h) : coro{h} {}
    AnyTask(AnyTask&& rhs) noexcept : coro{std::exchange(rhs.coro, nullptr)} {}

    void start() { coro.resume(); }

    std::coroutine_handle<> coro;
};

template <Awaitable... Tasks>
struct [[nodiscard]] Any : supp::NonCopyable {
    using ResultType = std::tuple<awaitable_result_t<Tasks>...>;
    using StateType = AnyState<awaitable_result_t<Tasks>...>;

    Any(Tasks... tasks) : tasks_(std::move(tasks)...) {}
    Any(Any&&) = default;

    // CancellableAwaitable
    Any& setCancellationSlot(CancellationSlot slot) {
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
            return state_.await_ready();
        }

        void await_suspend(std::coroutine_handle<> caller) { state_.await_suspend(caller); }

        ResultType await_resume() {
            return std::apply(
                [](auto&&... rs) { return ResultType(std::move(rs).get()...); },
                std::move(state_.result));
        }

     private:
        template <size_t I, typename Task>
        static AnyTask<awaitable_result_t<Task>, I, StateType> makeTask(
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

                return std::tuple<AnyTask<awaitable_result_t<Tasks>, Is, StateType>...>(
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

template <Awaitable... Tasks>
auto any(Tasks... tasks) {
    return detail::Any(std::move(tasks)...);
}

}  // namespace exec
