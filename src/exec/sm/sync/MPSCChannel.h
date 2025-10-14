#pragma once

#include "exec/Error.h"
#include "exec/executor/Executor.h"
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
class MPSCChannel;

template <typename T>
class ChanSendOp : public supp::IntrusiveListNode, CancellableOperation<ChanSendOp<T>> {
 public:
    ChanSendOp() = default;

    template <size_t Cap>
    [[nodiscard]] Initiator auto send(MPSCChannel<T, Cap>* chan, T value, ErrCode* ec) {
        value_ = stdlike::move(value);

        return [this, chan, ec](Runnable* cb, CancellationSlot slot = {}) {
            return chan->send(ec, cb, slot, this);
        };
    }

 private:
    // CancellableOperation::cancel
    void cancelHook() {
        unlink();
    }

    T value_;

    template <typename U, size_t Cap>
    friend class MPSCChannel;
    friend class CancellableOperation<ChanSendOp>;
};

// Multiple producers/single consumer
template <typename T, size_t Capacity>
class MPSCChannel : supp::Pinned {
 public:
    using OperationType = ChanSendOp<T>;

    MPSCChannel() = default;

    [[nodiscard]] Initiator auto receive(T* dest, ErrCode* ec) {
        return [this, dest, ec](Runnable* cb, CancellationSlot slot = {}) {
            return receive(dest, ec, cb, slot);
        };
    }

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
    struct ReceiveOp : CancellableOperation<> {
        T* dst = nullptr;
    };

    Runnable* send(ErrCode* ec, Runnable* cb, CancellationSlot slot, ChanSendOp<T>* op) {
        if (receiver_.outstanding()) {
            // Parked receiver => queue is empty
            DASSERT(size() == 0 || Capacity == 0);

            *receiver_.dst = stdlike::move(op->value_);
            executor()->post(receiver_.complete(ErrCode::Success));

            *ec = ErrCode::Success;
            return cb;
        }

        // No parked receiver, try to enqueue a new element
        if (!filled()) {
            buf_.push(stdlike::move(op->value_));
            *ec = ErrCode::Success;
            return cb;
        }

        // Channel is filled, park the sender
        op->initiate(ec, cb, slot);
        senders_.pushBack(op);
        return noop;
    }

    Runnable* receive(T* dest, ErrCode* ec, Runnable* cb, CancellationSlot slot) {
        if (!senders_.empty()) {
            // Parked sender => channel is full
            DASSERT(filled());

            *dest = buf_.pop();
            *ec = ErrCode::Success;
            executor()->post(cb);

            auto* sender = senders_.popFront();
            buf_.push(stdlike::move(sender->value_));
            return sender->complete(ErrCode::Success);
        }

        // No parked sender, try to dequeue an element
        if (size() > 0) {
            *dest = buf_.pop();
            *ec = ErrCode::Success;
            return cb;
        }

        // Channel is empty, park the receiver
        receiver_.dst = dest;
        receiver_.initiate(ec, cb, slot);
        return noop;
    }

    supp::CircularBuffer<T, Capacity> buf_;

    // Parked send operations (MP)
    supp::IntrusiveList<ChanSendOp<T>> senders_;

    // Parked receive operation (SC)
    ReceiveOp receiver_;

    friend class ChanSendOp<T>;
};

}  // namespace exec
