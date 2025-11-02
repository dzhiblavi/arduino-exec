#pragma once

#include "exec/Error.h"
#include "exec/Runnable.h"
#include "exec/cancel.h"

#include <supp/CircularBuffer.h>
#include <supp/IntrusiveList.h>
#include <supp/ManualLifetime.h>
#include <supp/NonCopyable.h>

#include <coroutine>
#include <cstddef>
#include <optional>
#include <utility>

namespace exec {

template <typename T, size_t Capacity>
class MPMCChannel {
 public:
    MPMCChannel() = default;

    auto receive() noexcept {
        return Receive{this};
    }

    auto send(T& value) noexcept {
        return Send{this, &value};
    }

 private:
    struct Awaitable : CancellationHandler, supp::IntrusiveListNode {
     public:
        Awaitable(MPMCChannel* self, CancellationSlot slot) : self{self}, slot{slot} {}

     protected:
        // CancellationHandler
        Runnable* cancel() override {
            slot.clearIfConnected();
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

        bool await_ready() noexcept {
            if (self->buf_.full() && !self->parked_.empty()) {
                value.emplace(self->buf_.pop());

                auto* sender = static_cast<SendAwaitable*>(self->parked_.popFront());
                sender->resume();
                return true;
            }

            if (!self->buf_.empty()) {
                value.emplace(self->buf_.pop());
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

        std::optional<T> await_resume() {
            return value.initialized() ? std::optional<T>{std::move(value).get()} : std::nullopt;
        }

        void resume(T* val) {
            slot.clearIfConnected();
            value.emplace(std::move(*val));
            caller.resume();
        }

        // CancellableAwaitable
        auto& setCancellationSlot(CancellationSlot a_slot) noexcept {
            slot = a_slot;
            return *this;
        }

     private:
        using Awaitable::caller;
        using Awaitable::self;
        using Awaitable::slot;

        supp::ManualLifetime<T> value;
    };

    struct SendAwaitable : Awaitable {
     public:
        SendAwaitable(MPMCChannel* self, T* value, CancellationSlot slot)
            : Awaitable(self, slot)
            , value{value} {}

        bool await_ready() const noexcept {
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

        void await_suspend(std::coroutine_handle<> a_caller) noexcept {
            caller = a_caller;
            slot.installIfConnected(this);
            self->parked_.pushBack(this);
        }

        ErrCode await_resume() const noexcept {
            // cancel() zeroes the self pointer
            return self == nullptr ? ErrCode::Cancelled : ErrCode::Success;
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
        Receive(Receive&&) noexcept = default;

        // CancellableAwaitable
        Receive& setCancellationSlot(CancellationSlot slot) noexcept {
            slot_ = slot;
            return *this;
        }

        auto operator co_await() noexcept {
            return ReceiveAwaitable{self_, slot_};
        }

     private:
        MPMCChannel* self_;
        CancellationSlot slot_{};
    };

    struct Send : supp::NonCopyable {
     public:
        Send(MPMCChannel* self, T* value) : self_{self}, value{value} {}
        Send(Send&&) noexcept = default;

        // CancellableAwaitable
        Send& setCancellationSlot(CancellationSlot slot) noexcept {
            slot_ = slot;
            return *this;
        }

        auto operator co_await() noexcept {
            return SendAwaitable{self_, value, slot_};
        }

     private:
        MPMCChannel* self_;
        T* value;
        CancellationSlot slot_{};
    };

    supp::CircularBuffer<T, Capacity> buf_;
    supp::IntrusiveList<Awaitable> parked_;
};

}  // namespace exec
