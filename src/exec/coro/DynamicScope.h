#pragma once

#include "exec/Unit.h"
#include "exec/cancel.h"
#include "exec/coro/traits.h"

#include <supp/IntrusiveList.h>
#include <supp/NonCopyable.h>
#include <supp/Pinned.h>

#include <coroutine>

namespace exec {

class DynamicScope {
    struct Promise : supp::IntrusiveListNode {
        using coroutine_handle_t = std::coroutine_handle<Promise>;

        template <Awaitable A>
        Promise(A&& awaitable, DynamicScope* scope) : scope{scope} {
            if constexpr (CancellableAwaitable<A>) {
                awaitable.setCancellationSlot(sig.slot());
            }
        }

        auto get_return_object() { return coroutine_handle_t::from_promise(*this); }
        auto initial_suspend() { return std::suspend_always{}; }

        void unhandled_exception() {
            LFATAL("unhandled exception in DynamicScope task");
            abort();
        }

        auto final_suspend() noexcept {
            struct Awaitable {
                bool await_ready() noexcept { return false; }
                void await_resume() noexcept {}

                std::coroutine_handle<> await_suspend(coroutine_handle_t self) noexcept {
                    Promise& promise = self.promise();
                    DynamicScope* scope = promise.scope;
                    DASSERT(scope, "dropped task has finished");

                    promise.unlink();
                    self.destroy();
                    return scope->arrived();
                }
            };

            return Awaitable{};
        }

        template <typename T>
        void return_value(T&& value) {
            // TODO: handle if needed
            (void)value;
        }

        void start() {
            DASSERT(scope);
            handle().resume();
        }

        void drop() {
            DASSERT(scope);
            scope = nullptr;
            handle().destroy();
        }

        void cancel() { sig.emit(); }
        coroutine_handle_t handle() { return coroutine_handle_t::from_promise(*this); }

        DynamicScope* scope;
        CancellationSignal sig{};
    };

    struct Task {
        using promise_type = Promise;
        using coroutine_handle_t = std::coroutine_handle<promise_type>;
        Task(coroutine_handle_t coro) : coro_{coro} {}
        Task(Task&& r) : coro_{std::exchange(r.coro_, nullptr)} {}
        Promise* promise() { return &coro_.promise(); }
        coroutine_handle_t coro_;
    };

    template <Awaitable A>
    static Task makeTask(A awaitable, DynamicScope* /*self*/) {
        // self is passed to Promise's constructor
        co_return co_await std::move(awaitable);
    }

    struct JoinAwaitable : CancellationHandler {
        JoinAwaitable(DynamicScope* self, CancellationSlot slot) : self_{self}, slot_{slot} {}

        bool await_ready() const { return self_->size() == 0; }

        Unit await_resume() {
            slot_.clearIfConnected();
            return unit;
        }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) {
            self_->tasks_.iterate([](Promise& task) { task.start(); });
            if (self_->size_ == 0) {
                return caller;
            }

            self_->caller_ = caller;
            slot_.installIfConnected(this);
            return std::noop_coroutine();
        }

     private:
        Runnable* cancel() override {
            DASSERT(self_->caller_ != nullptr);

            auto a_caller = std::exchange(self_->caller_, nullptr);
            self_->tasks_.iterate([](Promise& task) { task.cancel(); });

            if (self_->size_ == 0) {
                a_caller.resume();
            } else {
                self_->caller_ = a_caller;
            }

            return noop;
        }

        DynamicScope* self_;
        CancellationSlot slot_;
    };

 public:
    DynamicScope() = default;

    ~DynamicScope() {
        DASSERT(caller_ == nullptr);
        tasks_.iterate([](Promise& task) { task.drop(); });
    }

    size_t size() const { return size_; }

    template <Awaitable A>
    void add(A&& awaitable) {
        Task task = makeTask(std::forward<A>(awaitable), this);
        tasks_.pushBack(task.promise());
        ++size_;

        if (joining()) {
            // otherwise the join() awaitable will start tasks
            task.promise()->start();
        }
    }

    auto join() {
        struct Join {
            Join(DynamicScope* self) : self_{self} {}
            auto operator co_await() { return JoinAwaitable{self_, slot_}; }

            // CancellableAwaitable
            Join& setCancellationSlot(CancellationSlot slot) {
                slot_ = slot;
                return *this;
            }

         private:
            DynamicScope* self_;
            CancellationSlot slot_;
        };

        return Join{this};
    }

 private:
    bool joining() const { return caller_ != nullptr; }

    std::coroutine_handle<> arrived() {
        --size_;

        if (size_ > 0 || caller_ == nullptr) {
            return std::noop_coroutine();
        }

        return std::exchange(caller_, nullptr);
    }

    size_t size_ = 0;
    supp::IntrusiveList<Promise> tasks_;
    std::coroutine_handle<> caller_ = nullptr;
};

}  // namespace exec
