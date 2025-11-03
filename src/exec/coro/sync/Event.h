#pragma once

#include "exec/Error.h"
#include "exec/cancel.h"

#include <supp/IntrusiveList.h>
#include <supp/NonCopyable.h>
#include <supp/Pinned.h>

#include <coroutine>

namespace exec {

class Event : supp::Pinned {
 public:
    Event() = default;

    bool isSet() const { return fired_; }

    auto wait() { return Wait{this}; }

    void clear() { fired_ = false; }

    void fireOnce() { releaseAll(); }

    void set() {
        fired_ = true;
        releaseAll();
    }

 private:
    struct Awaitable : CancellationHandler, supp::IntrusiveListNode {
     public:
        Awaitable(Event* self, CancellationSlot slot) : self_{self}, slot_{slot} {}

        bool await_ready() { return self_->fired_; }

        // Locked, should park
        void await_suspend(std::coroutine_handle<> caller) {
            slot_.installIfConnected(this);
            caller_ = caller;
            self_->parked_.pushBack(this);
        }

        ErrCode await_resume() const {
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

        void fired() {
            slot_.clearIfConnected();
            caller_.resume();
        }

     private:
        Event* self_;
        CancellationSlot slot_;
        std::coroutine_handle<> caller_ = nullptr;
    };

    struct [[nodiscard]] Wait : supp::NonCopyable {
        Wait(Event* self) : self_{self} {}

        // CancellableAwaitable
        Wait& setCancellationSlot(CancellationSlot slot) {
            slot_ = slot;
            return *this;
        }

        auto operator co_await() { return Awaitable{self_, slot_}; }

     private:
        Event* self_;
        CancellationSlot slot_;
    };

    void releaseAll() {
        auto parked(std::move(parked_));

        while (!parked.empty()) {
            parked.popFront()->fired();
        }
    }

    supp::IntrusiveList<Awaitable> parked_;
    bool fired_ = false;
};

}  // namespace exec
