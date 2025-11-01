#pragma once

#include "exec/Error.h"
#include "exec/cancel.h"

#include <supp/IntrusiveList.h>
#include <supp/Pinned.h>

#include <coroutine>

namespace exec {

class Semaphore : supp::Pinned {
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
        return Parked{this};
    }

    void release() {
        if (counter_++ != 0 || parked_.empty()) {
            return;
        }

        --counter_;
        parked_.popFront()->takeSemaphore();
    }

 private:
    struct [[nodiscard]] Parked : CancellationHandler, supp::IntrusiveListNode, supp::Pinned {
        explicit Parked(Semaphore* self) : self_{self} {}

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

        // CancellableAwaitable
        Parked& setCancellationSlot(CancellationSlot slot) noexcept {
            slot_ = slot;
            return *this;
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
        std::coroutine_handle<> caller_;
        CancellationSlot slot_;
    };

    supp::IntrusiveList<Parked> parked_;
    int counter_ = 0;
};

}  // namespace exec
