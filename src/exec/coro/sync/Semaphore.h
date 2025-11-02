#pragma once

#include "exec/Error.h"
#include "exec/cancel.h"

#include <supp/IntrusiveList.h>
#include <supp/NonCopyable.h>

#include <coroutine>

namespace exec {

class Semaphore : supp::NonCopyable {
 public:
    explicit Semaphore(int init) : counter_{init} {}

    bool tryAcquire() {
        if (counter_ == 0) {
            return false;
        }

        --counter_;
        return true;
    }

    auto acquire() {
        return Acquire{this};
    }

    void release() {
        if (counter_++ != 0 || parked_.empty()) {
            return;
        }

        --counter_;
        parked_.popFront()->takeSemaphore();
    }

 private:
    struct Awaitable : CancellationHandler, supp::IntrusiveListNode {
        explicit Awaitable(Semaphore* self, CancellationSlot slot) : self_{self}, slot_{slot} {}

        bool await_ready() noexcept {
            return self_->tryAcquire();
        }

        void await_suspend(std::coroutine_handle<> caller) {
            slot_.installIfConnected(this);
            caller_ = caller;
            self_->parked_.pushBack(this);
        }

        ErrCode await_resume() const noexcept {
            return self_ == nullptr ? ErrCode::Cancelled : ErrCode::Success;
        }

        // CancellationHandler
        Runnable* cancel() override {
            slot_.clearIfConnected();
            unlink();
            self_ = nullptr;
            caller_.resume();
            return noop;
        }

        void takeSemaphore() {
            slot_.clearIfConnected();
            caller_.resume();
        }

        Semaphore* self_;
        CancellationSlot slot_;
        std::coroutine_handle<> caller_;
    };

    struct [[nodiscard]] Acquire : supp::NonCopyable {
     public:
        Acquire(Semaphore* self) : self_{self} {}

        // CancellableAwaitable
        Acquire& setCancellationSlot(CancellationSlot slot) noexcept {
            slot_ = slot;
            return *this;
        }

        auto operator co_await() noexcept {
            return Awaitable{self_, slot_};
        }

     private:
        Semaphore* self_;
        CancellationSlot slot_{};
    };

    supp::IntrusiveList<Awaitable> parked_;
    int counter_ = 0;
};

}  // namespace exec
