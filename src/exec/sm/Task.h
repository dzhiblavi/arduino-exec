#pragma once

#include "exec/cancel.h"
#include "exec/sm/Initiator.h"

#include <supp/Coro.h>

namespace exec {

class TaskBase : public Runnable, public supp::Coro {
 public:
    TaskBase() = default;
    TaskBase(TaskBase&& rhs) noexcept = default;

    // not copyable
    TaskBase(const TaskBase&) = delete;
    TaskBase& operator=(const TaskBase&) = delete;

    [[nodiscard]] Initiator auto start() {
        return [this](Runnable* cb, CancellationSlot slot = {}) {
            DASSERT(isComplete() || isInitial());

            reset();
            continuation_ = cb;
            slot_ = slot;

            // Implemented in concrete subclasses
            return run();
        };
    }

    Runnable* finish() {
        return stdlike::exchange(continuation_, noop);
    }

    CancellationSlot slot() const {
        return slot_;
    }

 private:
    Runnable* continuation_ = noop;
    CancellationSlot slot_;
};

using Ctx = TaskBase*;

template <typename Self>
class CoTask : public TaskBase {
 private:
    Runnable* run() final {
        return static_cast<Self*>(this)->body();
    }
};

template <typename T>
concept Task = requires(T& task) {
    { static_cast<TaskBase&>(task) };
};

template <typename T>
concept TaskBody = requires(T body, Ctx ctx) {
    { body(ctx) } -> stdlike::same_as<Runnable*>;
};

template <typename F>
concept TaskBodyFactory = requires(F func) {
    { func() } -> TaskBody;
};

template <typename F>
concept TaskFactory = requires(F func) {
    { func() } -> Task;
};

namespace detail {

template <TaskBody F>
struct TaskWrapper : TaskBase {
 public:
    TaskWrapper(F&& body) : body_(stdlike::forward<F>(body)) {}  // NOLINT

    Runnable* run() final {
        return body_(this);
    }

 private:
    stdlike::remove_reference_t<F> body_;
};

}  // namespace detail

template <Task T>
T task(T task) {
    return task;
}

template <TaskFactory Factory>
auto task(Factory factory) {
    return factory();
}

template <TaskBody Body>
auto task(Body&& body) {  // NOLINT
    return detail::TaskWrapper<Body&&>(stdlike::forward<Body>(body));
}

template <TaskBodyFactory Factory>
auto task(Factory factory) {
    return detail::TaskWrapper(factory());
}

template <typename T>
using TaskType = decltype(task(stdlike::declval<T>()));

}  // namespace exec

#define cobody(c) CORO_BODY_IMPL(c, return ::exec::noop)
#define coyield CORO_YIELD_IMPL(__LINE__)
#define coawait coyield return
#define coreturn(ctx) return ctx->finish()
