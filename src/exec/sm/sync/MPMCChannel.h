#pragma once

#include "exec/Error.h"
#include "exec/executor/global.h"
#include "exec/sm/Initiator.h"
#include "exec/sm/Operation.h"

#include <supp/CircularBuffer.h>
#include <supp/IntrusiveList.h>
#include <supp/Pinned.h>

#include <stdlike/array.h>
#include <stdlike/utility.h>

#include <log.h>

namespace exec {

template <typename T, size_t Capacity = 1>
class MPMCChannel;

template <typename T>
class ChanOp : public supp::IntrusiveListNode, CancellableOperation<ChanOp<T>> {
 public:
    ChanOp() = default;

    template <size_t Cap>
    [[nodiscard]] Initiator auto send(MPMCChannel<T, Cap>* chan, T value, ErrCode* ec) {
        value_ = stdlike::move(value);  // NOLINT

        return [this, chan, ec](Runnable* cb, CancellationSlot slot = {}) {
            return chan->send(ec, cb, slot, this);
        };
    }

    template <size_t Cap>
    [[nodiscard]] Initiator auto receive(MPMCChannel<T, Cap>* chan, T* dest, ErrCode* ec) {
        dest_ = dest;  // NOLINT

        return [this, chan, ec](Runnable* cb, CancellationSlot slot = {}) {
            return chan->receive(ec, cb, slot, this);
        };
    }

 private:
    // CancellableOperation::cancel
    void cancelHook() {
        unlink();
    }

    union {
        T value_;
        T* dest_;
    };

    template <typename U, size_t Cap>
    friend class MPMCChannel;
    friend class CancellableOperation<ChanOp>;
};

// Multiple producers/multiple consumers
template <typename T, size_t Capacity>
class MPMCChannel : supp::Pinned {
 public:
    using OperationType = ChanOp<T>;

    MPMCChannel() = default;

    size_t size() const {
        return buf_.size();
    }

    size_t free() const {
        return Capacity - size();
    }

    bool filled() const {
        return free() == 0;
    }

 private:
    Runnable* send(ErrCode* ec, Runnable* cb, CancellationSlot slot, ChanOp<T>* op) {
        if (size() == 0 && !parked_.empty()) {
            // Channel is empty and there are parked ops => parked receivers
            auto* receiver = parked_.popFront();
            *receiver->dest_ = stdlike::move(op->value_);
            executor()->post(receiver->complete(ErrCode::Success));

            *ec = ErrCode::Success;
            return cb;
        }

        // Try to enqueue the value
        if (!filled()) {
            buf_.push(stdlike::move(op->value_));
            *ec = ErrCode::Success;
            return cb;
        }

        // Channel is filled, park sender
        op->initiate(ec, cb, slot);
        parked_.pushBack(op);
        return noop;
    }

    Runnable* receive(ErrCode* ec, Runnable* cb, CancellationSlot slot, ChanOp<T>* op) {
        if (filled() && !parked_.empty()) {
            // Channel is filled and there are parked ops => parked senders
            DASSERT(filled());

            *op->dest_ = buf_.pop();
            *ec = ErrCode::Success;
            executor()->post(cb);

            auto* sender = parked_.popFront();
            buf_.push(stdlike::move(sender->value_));
            return sender->complete(ErrCode::Success);
        }

        // Try to dequeue an element
        if (size() > 0) {
            *op->dest_ = buf_.pop();
            *ec = ErrCode::Success;
            return cb;
        }

        // Channel is empty, park receiver
        op->initiate(ec, cb, slot);
        parked_.pushBack(op);
        return noop;
    }

    supp::CircularBuffer<T, Capacity> buf_;
    supp::IntrusiveList<ChanOp<T>> parked_;

    friend class ChanOp<T>;
};

}  // namespace exec
