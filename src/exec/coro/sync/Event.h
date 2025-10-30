#pragma once

#include "exec/Error.h"
#include "exec/cancel.h"

#include <supp/IntrusiveList.h>
#include <supp/Pinned.h>

#include <coroutine>

namespace exec {

class Event : supp::Pinned {
    struct Parked : CancellationHandler, supp::IntrusiveListNode {
        explicit Parked(Event* self) : self_{self} {}

        // not copyable
        Parked(const Parked&) = delete;
        Parked& operator=(const Parked&) = delete;

        Parked(Parked&& rhs) noexcept
            : self_{std::exchange(rhs.self_, nullptr)}
            , caller_(std::exchange(rhs.caller_, caller_))
            , slot_(rhs.slot_) {
            if (slot_.hasHandler()) {
                slot_.installIfConnected(this);
            }
        }

        bool await_ready() noexcept {
            if (!self_->fired_) {
                return false;
            }

            // operation is complete
            slot_.clearIfConnected();
            return true;
        }

        // Locked, should park
        void await_suspend(std::coroutine_handle<> caller) {
            caller_ = caller;
            self_->parked_.pushBack(this);
        }

        ErrCode await_resume() const noexcept {
            return self_ == nullptr ? ErrCode::Cancelled : ErrCode::Success;
        }

        // CancellableAwaitable
        Parked& setCancellationSlot(CancellationSlot slot) noexcept {
            slot_ = slot;
            slot_.installIfConnected(this);
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

        void fired() {
            slot_.clearIfConnected();
            caller_.resume();
        }

     private:
        Event* self_;
        std::coroutine_handle<> caller_;
        CancellationSlot slot_;
    };

 public:
    Event() = default;

    bool isSet() const {
        return fired_;
    }

    auto wait() {
        return Parked{this};
    }

    void set() {
        fired_ = true;
        releaseAll();
    }

    void clear() {
        fired_ = false;
    }

    void fireOnce() {
        releaseAll();
    }

 private:
    void releaseAll() {
        auto parked(std::move(parked_));

        while (!parked.empty()) {
            parked.popFront()->fired();
        }
    }

    supp::IntrusiveList<Parked> parked_;
    bool fired_ = false;
};

}  // namespace exec
