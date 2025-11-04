#pragma once

#include "exec/Error.h"
#include "exec/Result.h"
#include "exec/Runnable.h"
#include "exec/Unit.h"
#include "exec/cancel.h"

#include <supp/CircularBuffer.h>
#include <supp/IntrusiveList.h>
#include <supp/ManualLifetime.h>
#include <supp/NonCopyable.h>

#include <coroutine>
#include <cstddef>
#include <utility>

namespace exec {

template <typename T, size_t Capacity>
class MPMCChannel {
 public:
    MPMCChannel() = default;

    auto receive() { return Receive{this}; }

    auto send(T& value) { return Send{this, &value}; }

 private:
    struct Awaitable : CancellationHandler, supp::IntrusiveListNode {
     public:
        Awaitable(MPMCChannel* self, CancellationSlot slot) : self{self}, slot{slot} {}

     protected:
        // CancellationHandler
        Runnable* cancel() override {
            unlink();
            self = nullptr;
            caller.resume();
            return noop;
        }

        MPMCChannel* self;
        CancellationSlot slot;
        std::coroutine_handle<> caller = nullptr;
    };

    struct ReceiveAwaitable : Awaitable {
     public:
        using Awaitable::Awaitable;

        bool await_ready() {
            if (self->buf_.full() && !self->parked_.empty()) {
                result.emplace(self->buf_.pop());

                auto* sender = static_cast<SendAwaitable*>(self->parked_.popFront());
                sender->resume();
                return true;
            }

            if (!self->buf_.empty()) {
                result.emplace(self->buf_.pop());
                return true;
            }

            // buffer is empty and there is no awaiting sender: park
            return false;
        }

        void await_suspend(std::coroutine_handle<> a_caller) {
            caller = a_caller;
            slot.installIfConnected(this);
            self->parked_.pushBack(this);
        }

        Result<T> await_resume() {
            return result ? std::move(result) : Result<T>{ErrCode::Cancelled};
        }

        void resume(T* val) {
            slot.clearIfConnected();
            result.emplace(std::move(*val));
            caller.resume();
        }

     private:
        using Awaitable::caller;
        using Awaitable::self;
        using Awaitable::slot;

        Result<T> result;
    };

    struct SendAwaitable : Awaitable {
     public:
        SendAwaitable(MPMCChannel* self, T* value, CancellationSlot slot)
            : Awaitable(self, slot)
            , value{value} {}

        bool await_ready() const {
            if (self->buf_.empty() && !self->parked_.empty()) {
                auto* receiver = static_cast<ReceiveAwaitable*>(self->parked_.popFront());
                receiver->resume(value);
                return true;
            }

            if (!self->buf_.full()) {
                self->buf_.push(std::move(*value));
                return true;
            }

            // buffer is filled and there is no awaiting receiver: park
            return false;
        }

        void await_suspend(std::coroutine_handle<> a_caller) {
            caller = a_caller;
            slot.installIfConnected(this);
            self->parked_.pushBack(this);
        }

        Result<Unit> await_resume() const {
            // cancel() zeroes the self pointer
            return self == nullptr ? Result<Unit>{ErrCode::Cancelled} : Result<Unit>(unit);
        }

        void resume() {
            slot.clearIfConnected();
            self->buf_.push(std::move(*value));
            caller.resume();
        }

     private:
        using Awaitable::caller;
        using Awaitable::self;
        using Awaitable::slot;

        T* value;
    };

    struct Receive : supp::NonCopyable {
     public:
        Receive(MPMCChannel* self) : self_{self} {}
        Receive(Receive&&) = default;

        // CancellableAwaitable
        Receive& setCancellationSlot(CancellationSlot slot) {
            slot_ = slot;
            return *this;
        }

        auto operator co_await() { return ReceiveAwaitable{self_, slot_}; }

     private:
        MPMCChannel* self_;
        CancellationSlot slot_{};
    };

    struct Send : supp::NonCopyable {
     public:
        Send(MPMCChannel* self, T* value) : self_{self}, value{value} {}
        Send(Send&&) = default;

        // CancellableAwaitable
        Send& setCancellationSlot(CancellationSlot slot) {
            slot_ = slot;
            return *this;
        }

        auto operator co_await() { return SendAwaitable{self_, value, slot_}; }

     private:
        MPMCChannel* self_;
        T* value;
        CancellationSlot slot_{};
    };

    supp::CircularBuffer<T, Capacity> buf_;
    supp::IntrusiveList<Awaitable> parked_;
};

}  // namespace exec
