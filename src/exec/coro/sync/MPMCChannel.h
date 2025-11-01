#pragma once

#include "exec/Error.h"
#include "exec/Runnable.h"
#include "exec/cancel.h"

#include <supp/CircularBuffer.h>
#include <supp/IntrusiveList.h>
#include <supp/ManualLifetime.h>
#include <supp/Pinned.h>

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
        return ReceiveAwaitable{this};
    }

    auto send(T& value) noexcept {
        return SendAwaitable{this, value};
    }

 private:
    struct Awaitable : CancellationHandler, supp::Pinned, supp::IntrusiveListNode {
     public:
        explicit Awaitable(MPMCChannel* self) : self{self} {}

     protected:
        // CancellationHandler
        Runnable* cancel() override {
            slot.clearIfConnected();
            unlink();
            self = nullptr;
            caller.resume();
            return noop;
        }

        CancellationSlot slot;
        std::coroutine_handle<> caller = nullptr;
        MPMCChannel* self = nullptr;
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

        void resume(T& val) {
            slot.clearIfConnected();
            value.emplace(std::move(val));
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
        SendAwaitable(MPMCChannel* self, T& value) : Awaitable(self), value{value} {}

        bool await_ready() const noexcept {
            if (self->buf_.empty() && !self->parked_.empty()) {
                auto* receiver = static_cast<ReceiveAwaitable*>(self->parked_.popFront());
                receiver->resume(value);
                return true;
            }

            if (!self->buf_.full()) {
                self->buf_.push(std::move(value));
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
            self->buf_.push(std::move(value));
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

        T& value;
    };

    supp::CircularBuffer<T, Capacity> buf_;
    supp::IntrusiveList<Awaitable> parked_;
};

}  // namespace exec
