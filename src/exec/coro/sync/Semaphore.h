#pragma once

#include "exec/Error.h"
#include "exec/coro/cancel.h"
#include "exec/coro/traits.h"

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

    CancellableAwaitable auto acquire() { return Awaitable{this}; }

    void release() {
        if (counter_++ != 0 || parked_.empty()) {
            return;
        }

        --counter_;
        parked_.popFront()->takeSemaphore();
    }

 private:
    struct Awaiter : CancellationHandler, supp::IntrusiveListNode {
        explicit Awaiter(Semaphore* self, CancellationSlot slot) : self_{self}, slot_{slot} {}

        bool await_ready() { return self_->tryAcquire(); }

        void await_suspend(std::coroutine_handle<> caller) {
            slot_.installIfConnected(this);
            caller_ = caller;
            self_->parked_.pushBack(this);
        }

        ErrCode await_resume() const {
            return self_ == nullptr ? ErrCode::Cancelled : ErrCode::Success;
        }

        // CancellationHandler
        std::coroutine_handle<> cancel() override {
            unlink();
            self_ = nullptr;
            return caller_;
        }

        void takeSemaphore() {
            slot_.clearIfConnected();
            caller_.resume();
        }

        Semaphore* self_;
        CancellationSlot slot_;
        std::coroutine_handle<> caller_;
    };

    struct [[nodiscard]] Awaitable : supp::NonCopyable {
     public:
        Awaitable(Semaphore* self) : self_{self} {}

        // CancellableAwaitable
        Awaitable& setCancellationSlot(CancellationSlot slot) {
            slot_ = slot;
            return *this;
        }

        Awaiter operator co_await() { return Awaiter{self_, slot_}; }

     private:
        Semaphore* self_;
        CancellationSlot slot_{};
    };

    supp::IntrusiveList<Awaiter> parked_;
    int counter_ = 0;
};

}  // namespace exec
