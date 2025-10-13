#pragma once

#include "exec/executor/global.h"
#include "exec/sm/Initiator.h"

#include <supp/IntrusiveForwardList.h>
#include <supp/Pinned.h>

namespace exec {

class Mutex : supp::Pinned {
 public:
    Mutex() = default;

    bool tryLock() {
        if (locked_) {
            return false;
        }

        locked_ = true;
        return true;
    }

    [[nodiscard]] Initiator auto lock() {
        return [this](Runnable* cb, CancellationSlot /*ignored*/ = {}) {
            if (!locked_) {
                locked_ = true;
                return cb;
            }

            queue_.pushBack(cb);
            return noop;
        };
    }

    void unlock() {
        DASSERT(locked_);

        if (queue_.empty()) {
            locked_ = false;
            return;
        }

        executor()->post(queue_.popFront());
    }

 private:
    supp::IntrusiveForwardList<Runnable> queue_;
    bool locked_ = false;
};

}  // namespace exec
