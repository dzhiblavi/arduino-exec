#pragma once

#include "exec/Error.h"
#include "exec/executor/Executor.h"
#include "exec/sm/Initiator.h"
#include "exec/sm/Operation.h"

#include <supp/CircularBuffer.h>
#include <supp/Pinned.h>

#include <utility>

#include <log.h>

namespace exec {

// Single producer/single consumer
template <typename T, size_t Capacity = 1>
class SPSCChannel : supp::Pinned {
 public:
    SPSCChannel() = default;

    [[nodiscard]] Initiator auto send(T value, ErrCode* ec) {
        return [this, ec, value = std::move(value)](Runnable* cb, CancellationSlot slot = {}) {
            return send(std::move(value), ec, cb, slot);
        };
    }

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
    Runnable* send(T value, ErrCode* ec, Runnable* cb, CancellationSlot slot) {
        if (parked_.outstanding()) {
            // We are the sender, that makes parked_ a blocked receiver.
            DASSERT(size() == 0);

            *dst_ = std::move(value);  // NOLINT
            executor()->post(parked_.complete(ErrCode::Success));

            *ec = ErrCode::Success;
            return cb;
        }

        // No parked receiver, try to enqueue a new element
        if (!filled()) {
            buf_.push(std::move(value));
            *ec = ErrCode::Success;
            return cb;
        }

        // Buffer is filled, parking
        item_ = std::move(value);  // NOLINT
        parked_.initiate(ec, cb, slot);
        return noop;
    }

    Runnable* receive(T* dest, ErrCode* ec, Runnable* cb, CancellationSlot slot) {
        if (parked_.outstanding()) {
            // We are the receiver, that makes parked_ a blocked sender
            DASSERT(filled());

            *dest = buf_.pop();
            *ec = ErrCode::Success;
            executor()->post(cb);

            buf_.push(std::move(item_));  // NOLINT
            return parked_.complete(ErrCode::Success);
        }

        // No parked sender, try to dequeue an element
        if (size() > 0) {
            *dest = buf_.pop();
            *ec = ErrCode::Success;
            return cb;
        }

        // Buffer is empty, parking
        dst_ = dest;  // NOLINT
        parked_.initiate(ec, cb, slot);
        return noop;
    }

    union {
        T* dst_;
        T item_;
    };
    CancellableOperation<> parked_;
    supp::CircularBuffer<T, Capacity> buf_;
};

}  // namespace exec
